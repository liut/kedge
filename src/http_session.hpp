#pragma once


#include "net.hpp"

#include <optional>
#include <cstdlib>
#include <memory>      // enable_shared_from_this

namespace btd {

class httpCaller;

/** Represents an established HTTP connection
*/
class http_session : public std::enable_shared_from_this<http_session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<httpCaller> caller_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    std::optional<http::request_parser<http::string_body>> parser_;

    struct send_lambda;

    void fail(beast::error_code ec, char const* what);
    void do_read();
    void on_read(beast::error_code ec, std::size_t);
    void on_write(beast::error_code ec, std::size_t, bool close);

public:
    http_session(
        tcp::socket&& socket,
        std::shared_ptr<httpCaller> const& caller);

    void run();
};

} // namespace btd
