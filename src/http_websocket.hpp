#pragma once

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "config.h"
#include "net.hpp"
#include "log.hpp"
#include "util.hpp"

namespace btd {

using namespace std::literals;

// Forward declaration
class httpCaller;

/** Represents an active WebSocket connection to the server
*/
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
    beast::flat_buffer buffer_;
    websocket::stream<beast::tcp_stream> ws_;
    std::shared_ptr<httpCaller> caller_;
    std::vector<std::shared_ptr<std::string const>> queue_;
    std::string_view uri_;

    bool closed = false;

    void fail(beast::error_code ec, char const* what);
    void on_accept(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);

public:
    websocket_session(tcp::socket&& socket, std::shared_ptr<httpCaller> const& state);

    ~websocket_session();

    template<class Body, class Allocator>
    void
    run(http::request<Body, http::basic_fields<Allocator>> const& req);
    void
    close(const std::string_view msg = "quit");

    // Send a message
    void
    send(std::shared_ptr<std::string const> const& ss);

private:
    void
    on_send(std::shared_ptr<std::string const> const& ss);
};

template<class Body, class Allocator>
void
websocket_session::
run(http::request<Body, http::basic_fields<Allocator>> const& req)
{
    uri_ = req.target();
    std::clog << "ws run: " << uri_.data() << '\n';
    // Set suggested timeout settings for the websocket
    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res)
        {
            res.set(http::field::server, std::string(SERVER_SIGNATURE) + " websocket-kedge");
        }));

    ws_.control_callback([](auto kind, auto payload){
        LOG_DEBUG << "ws ctl kind " << asValue(kind) << " payload " << payload;
    });

    // Accept the websocket handshake
    ws_.async_accept(
        req,
        beast::bind_front_handler(&websocket_session::on_accept, shared_from_this()));
}

} // namespace btd
