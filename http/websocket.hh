//
// Created by hippolyteb on 3/9/17.
//

#ifndef SEASTARPLAYGROUND_WEBSOCKET_HPP
#define SEASTARPLAYGROUND_WEBSOCKET_HPP

#include <core/reactor.hh>
#include "request.hh"
#include "websocket_fragment.hh"
#include "websocket_message.hh"

namespace httpd {

class websocket_output_stream_base {
    output_stream<char> _stream;
public:
    websocket_output_stream_base() = default;

    websocket_output_stream_base(output_stream<char>&& stream) : _stream(std::move(stream)) {}

    websocket_output_stream_base(websocket_output_stream_base &&) = default;

    websocket_output_stream_base &operator=(websocket_output_stream_base &&) = default;

    future<> close() { return _stream.close(); };
protected:
    future<> write(httpd::websocket_message_base message);
    friend class reactor;
};

template<websocket_type type>
class websocket_output_stream final : public websocket_output_stream_base {
using websocket_output_stream_base::websocket_output_stream_base;
public:
    future<> write(httpd::websocket_message<type> message) {
        message.done();
        return websocket_output_stream_base::write(std::move(message));
    };
};

class websocket_input_stream_base {
protected:
    input_stream<char> _stream;
    temporary_buffer<char> _buf;
    uint32_t _index = 0;

public:
    websocket_input_stream_base() = default;

    websocket_input_stream_base(input_stream<char>&& stream) : _stream(std::move(stream)) {}

    websocket_input_stream_base(websocket_input_stream_base &&) = default;

    websocket_input_stream_base &operator=(websocket_input_stream_base &&) = default;

    future<> close() { return _stream.close(); }
};

    template<websocket_type type>
class websocket_input_stream final : public websocket_input_stream_base {
using websocket_input_stream_base::websocket_input_stream_base;
private:
    inbound_websocket_fragment<type> _fragment;
    websocket_message<type> _lastmassage;
public:
    future<> read_fragment() {
        auto parse_fragment = [this] {
            if (_buf.size() - _index > 2)
                _fragment.parse(_buf, &_index);
        };

        _fragment.reset();
        if (!_buf || _index >= _buf.size())
            return _stream.read().then([this, parse_fragment](temporary_buffer<char> buf) {
                _buf = std::move(buf);
                _index = 0;
                parse_fragment();
            });
        parse_fragment();
        return make_ready_future();
    }

    future<httpd::websocket_message<type>> read() {
        _lastmassage.reset();
        return repeat([this] { // gather all fragments and concatenate full message
            return read_fragment().then([this] {
                if (!_fragment)
                    return stop_iteration::yes;
                else if (_fragment.fin) {
                    if (!_lastmassage)
                        _lastmassage = websocket_message<type>(std::move(_fragment));
                    else
                        _lastmassage.append(std::move(_fragment));
                    return stop_iteration::yes;
                } else if (_fragment.opcode() == CONTINUATION)
                    _lastmassage.append(std::move(_fragment));
                return stop_iteration::no;
            });
        }).then([this] {
            return std::move(_lastmassage);
        });
    }

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

    websocket_input_stream<type> input() {
        return websocket_input_stream<type>(std::move(_socket.input()));
    }

    websocket_output_stream<type> output() {
        return websocket_output_stream<type>(std::move(_socket.output()));
    }

    void shutdown_output() { _socket.shutdown_output(); }

    void shutdown_input() { _socket.shutdown_input(); }
};
    sstring generate_websocket_key(sstring nonce);
    future<connected_websocket<websocket_type::CLIENT>> connect_websocket(socket_address sa, socket_address local = socket_address(::sockaddr_in{AF_INET, INADDR_ANY, {0}}));
}

#endif //SEASTARPLAYGROUND_WEBSOCKET_HPP