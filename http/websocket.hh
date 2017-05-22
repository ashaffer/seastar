//
// Created by hippolyteb on 3/9/17.
//

#pragma once

#include <core/reactor.hh>
#include "request.hh"
#include "websocket_fragment.hh"
#include "websocket_message.hh"

namespace httpd {

class websocket_output_stream_base {
private:
    output_stream<char> _stream;
public:

    websocket_output_stream_base() noexcept = delete;
    websocket_output_stream_base(output_stream<char>&& stream) noexcept: _stream(std::move(stream)) { }
    websocket_output_stream_base(websocket_output_stream_base &&) noexcept = default;
    websocket_output_stream_base &operator=(websocket_output_stream_base &&) noexcept = default;

    future<> close() { return _stream.close(); };
    future<> flush() { return _stream.flush(); };

protected:
    future<> write(temporary_buffer<char> header, httpd::websocket_message_base message);
    friend class reactor;
};

template<websocket_type type>
class websocket_output_stream final : public websocket_output_stream_base {
using websocket_output_stream_base::websocket_output_stream_base;
public:
    future<> write(httpd::websocket_message<type> message) {
        auto header = message.get_header();
        return websocket_output_stream_base::write(std::move(header), std::move(message));
    };
};

class websocket_input_stream_base {
protected:
    input_stream<char> _stream;

public:
    websocket_input_stream_base() noexcept = delete;
    websocket_input_stream_base(input_stream<char>&& stream) noexcept: _stream(std::move(stream)) { }
    websocket_input_stream_base(websocket_input_stream_base &&) noexcept = default;
    websocket_input_stream_base &operator=(websocket_input_stream_base &&) noexcept = default;

    future<> close() { return _stream.close(); }
};

template<websocket_type type>
class websocket_input_stream final : public websocket_input_stream_base {
using websocket_input_stream_base::websocket_input_stream_base;
private:
    std::vector<inbound_websocket_fragment<type>> _fragmented_message;
    websocket_message<type> _message;

public:
    future<inbound_websocket_fragment<type>> read_fragment() {
        return _stream.read_exactly(sizeof(uint16_t)).then([this] (temporary_buffer<char>&& header) {
            if (!header)
                throw websocket_exception(NORMAL_CLOSURE);
            websocket_fragment_header fragment_header(header);
            if (fragment_header.extended_header_size() > 0) {
                return _stream.read_exactly(fragment_header.extended_header_size()).then([this, fragment_header] (temporary_buffer<char> extended_header) mutable {
                    if (!extended_header)
                        throw websocket_exception(NORMAL_CLOSURE);
                    fragment_header.feed_extended_header(extended_header);
                    return _stream.read_exactly(fragment_header.length).then([this, fragment_header] (temporary_buffer<char>&& payload) {
                        if (!payload && fragment_header.length > 0)
                            throw websocket_exception(NORMAL_CLOSURE);
                        return inbound_websocket_fragment<type>(fragment_header, payload);
                    });
                });
            }
            return _stream.read_exactly(fragment_header.length).then([this, fragment_header] (temporary_buffer<char>&& payload) {
                if (!payload && fragment_header.length > 0)
                    throw websocket_exception(NORMAL_CLOSURE);
                return inbound_websocket_fragment<type>(fragment_header, payload);
            });
        });
    }

    future<httpd::websocket_message<type>> read() {
        return repeat([this] { // gather all fragments
            return read_fragment().then([this](inbound_websocket_fragment<type> &&fragment) {
                if (!fragment) { throw websocket_exception(PROTOCOL_ERROR); }
                switch (fragment.header.opcode) {
                    case CONTINUATION:
                    {
                        if (!_fragmented_message.empty()) { _fragmented_message.emplace_back(std::move(fragment)); }
                        else { throw websocket_exception(PROTOCOL_ERROR); } //protocol error, close connection
                        if (fragment.header.fin) {
                            _message = websocket_message<type>(_fragmented_message);
                            _fragmented_message.clear();
                        }
                        return stop_iteration(fragment.header.fin);
                    }

                    case TEXT:
                    case BINARY:
                    {
                        if (fragment.header.fin && _fragmented_message.empty()) {
                            _message = websocket_message<type>(fragment);
                        }
                        else if (!fragment.header.fin && _fragmented_message.empty()) {
                            _fragmented_message.emplace_back(std::move(fragment));
                        }
                        else { throw websocket_exception(PROTOCOL_ERROR); } //protocol error, close connection
                        return stop_iteration(fragment.header.fin);
                    }

                    case PING:
                    case PONG:
                    {
                        if (fragment.header.fin) { _message = websocket_message<type>(fragment); }
                        else { throw websocket_exception(PROTOCOL_ERROR); } //protocol error, close connection
                        return stop_iteration::yes;
                    }

                    case CLOSE: //remote pair asked for close
                        throw websocket_exception(NONE); //protocol error, close connection

                    case RESERVED:
                    default:
                        throw websocket_exception(UNEXPECTED_CONDITION); //Hum.. this is embarrassing
                }
            });
        }).then([this] {
            return std::move(_message);
        });
    }
};

//TODO Should specialize this class to implement SERVER/CLIENT close behavior. Only SERVER closing is tested
template<websocket_type type>
class websocket_stream {
private:
    websocket_input_stream<type> _input_stream;
    websocket_output_stream<type> _output_stream;

public:
    websocket_stream(websocket_input_stream<type>&& input_stream,
                     websocket_output_stream<type>&& output_stream) noexcept:
            _input_stream(std::move(input_stream)),
            _output_stream(std::move(output_stream)) { }

    websocket_stream(websocket_stream &&) noexcept = default;
    websocket_stream &operator=(websocket_stream &&) noexcept = default;

    future<httpd::websocket_message<type>> read() {
        return _input_stream.read().handle_exception_type([this] (websocket_exception& ex) {
            return close(ex.status_code).then([ex = std::move(ex)] () -> future<httpd::websocket_message<type>> {
                return make_exception_future<httpd::websocket_message<type>>(ex);
            });
        }).then([] (websocket_message<type> message) {
            return make_ready_future<httpd::websocket_message<type>>(std::move(message));
        });
    }

    future<> write(httpd::websocket_message<type> message) {
        return _output_stream.write(std::move(message));
    };

    future<> close(websocket_close_status_code code = NORMAL_CLOSURE) {
        return write(websocket_message<type>::make_close_message(code)).then([this] {
            return _output_stream.flush();
        }).finally([this] {
            return when_all(_input_stream.close(), _output_stream.close()).discard_result();
        });
    };

    future<> flush() { return _output_stream.flush(); };
};

template<websocket_type type>
class connected_websocket {
private:
    connected_socket _socket;

public:
    socket_address remote_adress;

    connected_websocket(connected_socket socket, const socket_address remote_adress) noexcept :
            _socket(std::move(socket)), remote_adress(remote_adress) {
    }

    connected_websocket(connected_websocket &&cs) noexcept : _socket(
            std::move(cs._socket)), remote_adress(cs.remote_adress) {
    }

    connected_websocket &operator=(connected_websocket &&cs) noexcept {
        _socket = std::move(cs._socket);
        remote_adress = std::move(cs.remote_adress);
        return *this;
    };

    websocket_stream<type> stream() {
        return websocket_stream<type>(websocket_input_stream<type>(std::move(_socket.input())),
                                      websocket_output_stream<type>(std::move(_socket.output())));
    }

    void shutdown_output() { _socket.shutdown_output(); }
    void shutdown_input() { _socket.shutdown_input(); }
};
    sstring generate_websocket_key(sstring nonce);
    future<connected_websocket<websocket_type::CLIENT>> connect_websocket(socket_address sa, socket_address local = socket_address(::sockaddr_in{AF_INET, INADDR_ANY, {0}}));
}