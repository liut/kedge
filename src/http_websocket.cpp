

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/json/serialize.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>

#include "http_websocket.hpp"
#include "handlers.hpp"
#include "json_diff.hpp"
#include "util.hpp"
#include "log.hpp"

namespace btd {

// class httpCaller;

websocket_session::
websocket_session(
    tcp::socket&& socket,
    std::shared_ptr<httpCaller> const& caller)
    : ws_(std::move(socket))
    , caller_(caller)
{
}

websocket_session::
~websocket_session()
{
    closed = true;
    LOG_DEBUG << "ws destroying";
    // Remove this session from the list of active sessions
    caller_->leave(this);
}

void
websocket_session::
fail(beast::error_code ec, char const* what)
{
    // Don't report these
    if( ec == net::error::operation_aborted ||
        ec == websocket::error::closed)
        return;

    LOG_WARNING << what << ": " << ec.message() << "\n";
}

void
websocket_session::
on_accept(beast::error_code ec)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "ws on_accept");

    // Add this session to the list of active sessions
    caller_->join(this);

    // Read a message
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));

}

void
websocket_session::
on_read(beast::error_code ec, std::size_t)
{
    if(ec == websocket::error::closed) closed = true;
    // Handle the error, if any
    if(ec)
        return fail(ec, "ws on_read");
    if(closed) return;

    // Send to all connections
    caller_->broadcast(beast::buffers_to_string(buffer_.data()));

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Read another message
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));
}

void
websocket_session::
send(std::shared_ptr<std::string const> const& ss)
{
    // Post our work to the strand, this ensures
    // that the members of `this` will not be
    // accessed concurrently.
    net::post(
        ws_.get_executor(),
        beast::bind_front_handler(
            &websocket_session::on_send,
            shared_from_this(),
            ss));
}

void
websocket_session::
on_send(std::shared_ptr<std::string const> const& ss)
{
    // Always add to queue
    queue_.push_back(ss);

    // Are we already writing?
    if(queue_.size() > 1)
        return;

    // We are not currently writing, so send this immediately
    ws_.async_write(
        net::buffer(*queue_.front()),
        beast::bind_front_handler(
            &websocket_session::on_write,
            shared_from_this()));
}

void
websocket_session::
on_write(beast::error_code ec, std::size_t)
{
    if (ec == websocket::error::closed)
    {
       close("ws Write error");
       return;
    }
    // Handle the error, if any
    if(ec)
        return fail(ec, "ws on_write");

    // Remove the string from the queue
    queue_.erase(queue_.begin());

    // Send the next message if any
    if(! queue_.empty())
        ws_.async_write(
            net::buffer(*queue_.front()),
            beast::bind_front_handler(
                &websocket_session::on_write,
                shared_from_this()));
}

void
websocket_session::
close(const std::string_view msg)
{
    ws_.async_close(
        {websocket::close_code::normal, msg},
        [self(shared_from_this())](boost::system::error_code ec) {
            if (ec == net::error::operation_aborted)
            {
                return;
            }
            if (ec)
            {
                LOG_ERROR << "Error closing websocket " << ec;
                return;
            }
        });
    closed = true;
    // websocket::close_reason cr("direct close");
    // ws_.close(cr);
}


} // namespace btd
