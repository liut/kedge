#pragma once

#include <boost/asio/coroutine.hpp>
#include <boost/beast.hpp>
#include <boost/optional.hpp>
#include <iostream>

#include "web/net.hpp"

namespace web {
template <class Body> class HttpServer;

template <class Body>
class Session
        : public net::coroutine
        , public std::enable_shared_from_this<Session<Body>> {
public:
    Session(std::shared_ptr<HttpServer<Body>> server, tcp::socket && socket)
            : stream(std::move(socket)), server(std::move(server)) { }

    void run() {
        do_read(false, {}, 0);
    }

private:
#include <boost/asio/yield.hpp>
    void do_read(bool close,
                 beast::error_code ec,
                 std::size_t bytes_transferred) {
        reenter(*this) {
            for(;;) {
                parser.emplace();
                parser->body_limit(10'000);
                stream.expires_after(std::chrono::seconds(10));

                yield http::async_read(stream, buffer, parser->get(), beast::bind_front_handler(&Session::do_read, this->shared_from_this(), false));

                if (ec == http::error::end_of_stream) {
                    break;
                }

                if (ec) {
                    std::cerr << " async read err: " << ec.message();
                    break;
                }

                response = std::make_shared<http::response<http::string_body>>
                        (std::move(server->requestHandler(std::move(parser->release()))));

                yield http::async_write(stream, *response, beast::bind_front_handler(&Session::do_read, this->shared_from_this(), response->need_eof()));

                if (ec) {
                    std::cerr << " async write err: " << ec.message();
                    return;
                }

                if (close) {
                    break;
                }

                response.reset();
            }

            do_close();
        }
    }
#include <boost/asio/unyield.hpp>

    void do_close() {
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    std::shared_ptr<http::response<http::string_body>> response;

    beast::tcp_stream stream;
    beast::flat_buffer buffer;
    boost::optional<http::request_parser<Body>> parser;
    std::shared_ptr<HttpServer<Body>> server;
};

}
