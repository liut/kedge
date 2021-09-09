
#include "net.hpp"
#include "http_util.hpp"
#include "http_session.hpp"
#include "http_websocket.hpp"
#include "handlers.hpp"

namespace btd {

// Return a reasonable mime type based on the extension of a file.
std::string_view
mime_type(std::string_view path)
{
    using beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == std::string_view::npos)
            return std::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    std::shared_ptr<httpCaller> const& caller,
    request<Body, basic_fields<Allocator>>&& req,
    Send&& send)
{
    // Returns a bad request response
    auto const bad_request = [&req](std::string_view why)
    {
        return make_resp<string_body>(req, std::string(why), ctText, status::bad_request);
    };

    // Returns a not found response
    auto const not_found = [&req](std::string_view target)
    {
        return make_resp<string_body>(req, "The resource '" + std::string(target) + "' was not found."
        	, ctText, status::not_found);
    };

    // Returns a server error response
    auto const server_error = [&req](std::string_view what)
    {
        return make_resp<string_body>(req, "An error occurred: '" + std::string(what) + "'"
        	, ctText, status::internal_server_error);
    };

    // auto const

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != std::string_view::npos)
        return send(bad_request("Illegal request-target"));

    auto opt_resp = caller->sbCall(req);
    if (opt_resp) return send(opt_resp.value());

    // Build the path to the requested file
    std::string path = path_cat(caller->ui_root(), req.target());
    if(req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    beast::error_code ec;
    file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if(ec == boost::system::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    // Handle an unknown error
    if(ec)
        return send(server_error(ec.message()));

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if(req.method() == verb::head)
    {
        response<empty_body> res{status::ok, req.version()};
        res.set(field::server, SERVER_SIGNATURE);
        res.set(field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    response<file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(status::ok, req.version())};
    res.set(field::server, SERVER_SIGNATURE);
    res.set(field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}

//------------------------------------------------------------------------------

http_session::
http_session(
    tcp::socket&& socket,
    std::shared_ptr<httpCaller> const& caller)
    : stream_(std::move(socket))
    , caller_(caller)
{
}

void
http_session::
run()
{
    do_read();
}

// Report a failure
void
http_session::
fail(beast::error_code ec, char const* what)
{
    // Don't report on canceled operations
    if(ec == net::error::operation_aborted)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

void
http_session::
do_read()
{
    // Construct a new parser for each message
    parser_.emplace();

    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    parser_->body_limit(10000);

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    // Read a request
    http::async_read(
        stream_,
        buffer_,
        parser_->get(),
        beast::bind_front_handler(
            &http_session::on_read,
            shared_from_this()));
}

void
http_session::
on_read(beast::error_code ec, std::size_t)
{
    // This means they closed the connection
    if(ec == http::error::end_of_stream)
    {
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        return;
    }

    // Handle the error, if any
    if(ec)
        return fail(ec, "read");

    // See if it is a WebSocket Upgrade
    if(websocket::is_upgrade(parser_->get()))
    {
        // Create a websocket session, transferring ownership
        // of both the socket and the HTTP request.
        std::make_shared<websocket_session>(
            stream_.release_socket(),
                caller_)->run(parser_->release());
        return;
    }

    //
    // The following code requires generic
    // lambdas, available in C++14 and later.
    //
    handle_request(
        caller_,
        parser_->release(),
        [this](auto&& response)
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            using response_type = typename std::decay<decltype(response)>::type;
            auto sp = std::make_shared<response_type>(std::forward<decltype(response)>(response));

            // Write the response
            auto self = shared_from_this();
            http::async_write(stream_, *sp,
                [self, sp](
                    beast::error_code ec, std::size_t bytes)
                {
                    self->on_write(ec, bytes, sp->need_eof());
                });

        });

}

void
http_session::
on_write(beast::error_code ec, std::size_t, bool close)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "write");

    if(close)
    {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        return;
    }

    // Read another request
    do_read();
}

} // namespace btd
