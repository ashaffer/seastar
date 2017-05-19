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
    std::vector<inbound_websocket_fragment<type>> _fragmented_message;
    websocket_message<type> _message;
public:
    future<inbound_websocket_fragment<type>> read_fragment() {
        if (!_buf || _index >= _buf.size()) {
          return _stream.read().then([this] (temporary_buffer<char>&& buf) {
            _buf = std::move(buf);
            _index = 0;
            return inbound_websocket_fragment<type>(_buf, &_index);
          });
        }
        return make_ready_future<inbound_websocket_fragment<type>>(std::move(inbound_websocket_fragment<type>(_buf, &_index)));
    }

    future<httpd::websocket_message<type>> read() {
        _fragmented_message.clear();
        return repeat([this] { // gather all fragments
            return read_fragment().then([this](inbound_websocket_fragment<type> &&fragment) {
                if (!fragment)
                    throw std::exception();
                else if (fragment.opcode() > 0x2) {
                    _message = websocket_message<type>(fragment);
                    return stop_iteration::yes;
                }
                else if (fragment.fin) {
                    if (_fragmented_message.size() > 0) {
                        _fragmented_message.push_back(std::move(fragment));
                        _message = websocket_message<type>(_fragmented_message);
                    } else
                        _message = websocket_message<type>(fragment);
                    return stop_iteration::yes;
                } else if (fragment.opcode() == CONTINUATION)
                    _fragmented_message.push_back(std::move(fragment)); //fixme emplace_back would be nice
                else
                    return stop_iteration::yes;
                return stop_iteration::no;
            });
        }).then([this] {
            return std::move(_message);
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