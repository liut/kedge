#pragma once

#include <boost/beast/core/error.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <memory>

#include "web/session.hpp"

namespace web {
template <class Body>
class Listener : public net::coroutine, public std::enable_shared_from_this<Listener<Body>> {
public:
    Listener(net::io_context &io_context,
            const tcp::endpoint& endpoint,
            std::shared_ptr<HttpServer<Body>> server)
    : io_context(io_context)
    , acceptor(net::make_strand(io_context))
    , socket(net::make_strand(io_context))
    , server(std::move(server)) {
        acceptor.open(endpoint.protocol());
        acceptor.set_option(net::socket_base::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen(net::socket_base::max_listen_connections);
    }

    void run() {
        accept();
    }

    void stop() {
        acceptor.cancel();
        acceptor.close();
    }

private:
#include <boost/asio/yield.hpp>
    void accept(boost::beast::error_code ec = {}) {
        reenter(*this) {
            for (;;) {
                yield acceptor.async_accept(
                        socket,
                        [&] (boost::system::error_code ec) {
                            accept(ec);
                        });

                if (!ec) {
                    std::make_shared<Session<Body>>(server, std::move(socket))->run();
                }

                socket = tcp::socket(net::make_strand(io_context));
            }
        }
    }
#include <boost/asio/unyield.hpp>

    net::io_context &io_context;
    tcp::acceptor acceptor;
    tcp::socket socket;
    std::shared_ptr<HttpServer<Body>> server;
};

}

