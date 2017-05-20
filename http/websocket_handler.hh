//
// Created by hbarraud on 4/1/17.
//

#ifndef SEASTAR_WEBSOCKET_HANDLER_HH
#define SEASTAR_WEBSOCKET_HANDLER_HH

#include "handlers.hh"

namespace httpd {

template<websocket_type type>
class websocket_function_handler : public httpd::handler_websocket_base {
typedef std::function<future<>(std::unique_ptr<request>, connected_websocket<type>)> future_ws_handler_function;
public:
    websocket_function_handler(const future_ws_handler_function & f_handle)
            : _f_handle(f_handle) {
    }

    future<> handle(const sstring &path, connected_websocket<type> ws, std::unique_ptr<request> request) override {
        return _f_handle(std::move(request), std::move(ws));
    }

protected:
    future_ws_handler_function _f_handle;
};

template<websocket_type type>
class websocket_handler : public httpd::handler_websocket_base {
typedef std::function<future<>(const std::unique_ptr<request>&)> future_ws_on_disconnected;
typedef std::function<future<>(const std::unique_ptr<request>&, websocket_output_stream<type> &)> future_ws_on_connected;
typedef std::function<future<>(const std::unique_ptr<request>&, websocket_output_stream<type> &, httpd::websocket_message<type>)> future_ws_on_message;

typedef std::function<void(const std::unique_ptr<request>&)> void_ws_on_disconnected;
typedef std::function<void(const std::unique_ptr<request>&)> void_ws_on_connected;
typedef std::function<void(const std::unique_ptr<request>&, httpd::websocket_message<type> message)> void_ws_on_message;

public:
    websocket_handler() : _on_connection([] (const std::unique_ptr<request>&, websocket_output_stream<type> &) { return make_ready_future(); }),
                          _on_message([] (const std::unique_ptr<request>&, websocket_output_stream<type> &, httpd::websocket_message<type> message) { return make_ready_future(); }),
                          _on_disconnection([] (const std::unique_ptr<request>&) { return make_ready_future(); }),
                          _on_pong([] (const std::unique_ptr<request>&, websocket_output_stream<type> &, httpd::websocket_message<type> message) { return make_ready_future(); }),
                          _on_ping([] (const std::unique_ptr<request>&, websocket_output_stream<type>& output, httpd::websocket_message<type> message) {
                              message.opcode = websocket_opcode::PONG;
                              return output.write(std::move(message));
                          })
    {

    }

    future<> handle(const sstring &path, connected_websocket<SERVER> ws, std::unique_ptr<request> req) override {
        auto input = ws.input();
        auto output = ws.output();
        return do_with(std::move(ws), std::move(input), std::move(output), std::move(req),
                       [this] (connected_websocket<type> &ws, websocket_input_stream<type> &input,
                               websocket_output_stream<type> &output, std::unique_ptr<request>& req) {
            return _on_connection(req, output).then([this, &input, &output, &req] {
                return repeat([this, &input, &output, &req] {
                    return input.read().then([this, &req, &output](httpd::websocket_message<type> message) {
                        if (!message)
                            return make_ready_future<bool_class<stop_iteration_tag>>(stop_iteration::yes);
                        return on_message_internal(req, output, message).then([] (bool close) {
                            if (close)
                                return make_ready_future<bool_class<stop_iteration_tag>>(stop_iteration::yes);
                            return make_ready_future<bool_class<stop_iteration_tag>>(stop_iteration::no);
                        });
                    });
                });
            }).finally([this, &req, &input, &output] {
                _on_disconnection(req);
                return when_all(input.close(), output.close());
            });
        });
    }

    void on_message(const void_ws_on_message & handler) {
        _on_message = [handler](const std::unique_ptr<request>& req, websocket_output_stream<type> &, httpd::websocket_message<type> message) {
            handler(req, std::move(message));
            return make_ready_future();
        };
    }
    void on_ping(const void_ws_on_message & handler) {
        _on_ping = [handler](const std::unique_ptr<request>& req, websocket_output_stream<type> &, httpd::websocket_message<type> message) {
            handler(req, std::move(message));
            return make_ready_future();
        };
    }
    void on_pong(const void_ws_on_message & handler) {
        _on_pong = [handler](const std::unique_ptr<request>& req, websocket_output_stream<type> &, httpd::websocket_message<type> message) {
            handler(req, std::move(message));
            return make_ready_future();
        };
    }
    void on_connection(const void_ws_on_connected & handler) {
        _on_connection = [handler](const std::unique_ptr<request>& req, websocket_output_stream<type> &) {
            handler(req);
            return make_ready_future();
        };
    }
    void on_disconnection(const void_ws_on_disconnected & handler) {
        _on_disconnection = [handler](const std::unique_ptr<request>& req) {
            handler(req);
            return make_ready_future();
        };
    }

    void on_message_future(const future_ws_on_message & handler) { _on_message = handler; }
    void on_ping_future(const future_ws_on_message & handler) { _on_ping = handler; }
    void on_pong_future(const future_ws_on_message & handler) { _on_pong = handler; }
    void on_connection_future(const future_ws_on_connected & handler) { _on_connection = handler; }
    void on_disconnection_future(const future_ws_on_disconnected & handler) { _on_disconnection = handler; }

private:

    future<bool> on_message_internal(const std::unique_ptr<request>& req,
                                     websocket_output_stream<type>& output,
                                     httpd::websocket_message<type>& message) {
        switch (message.opcode){
            case TEXT:
                //FIXME Check that buffer is valid UTF-8
            case BINARY:
                return _on_message(req, output, std::move(message)).then([] {
                    return false;
                });
            case PING:
                return _on_ping(req, output, std::move(message)).then([] {
                    return false;
                });
            case PONG:
                return _on_pong(req, output, std::move(message)).then([] {
                    return true;
                });
            case CLOSE:
            case RESERVED:
            default:
                return make_ready_future<bool>(true);
        }
    }

protected:
    future_ws_on_connected _on_connection;
    future_ws_on_message _on_message;
    future_ws_on_disconnected _on_disconnection;
    future_ws_on_message _on_pong;
    future_ws_on_message _on_ping;
};

}


#endif //SEASTAR_WEBSOCKET_HANDLER_HH
