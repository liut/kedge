#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include "net.hpp"

namespace btd {

// Forward declaration
class httpCaller;

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<httpCaller> caller_;

    void fail(beast::error_code ec, char const* what);
    void on_accept(beast::error_code ec, tcp::socket socket);

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<httpCaller> const& caller);

    // Start accepting incoming connections
    void run();
};

} // namespace btd
