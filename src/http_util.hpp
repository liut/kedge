#ifndef _HTTP_UTIL_HPP
#define _HTTP_UTIL_HPP

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <string_view>

#include "config.h"
#include "net.hpp"

namespace btd {

using namespace http;
using namespace std::literals::string_view_literals;

auto const ctJSON = "application/json"sv;
auto const ctText = "text/plain"sv;

auto const hdCacheControl = "no-cache, no-store, must-revalidate"sv;
auto const hdPragma = "no-cache"sv;

template<class ResponseBody, class RequestBody>
auto
make_resp(const request<RequestBody>& req
	, const typename ResponseBody::value_type body
	, const std::string_view ct, const status sc = status::ok)
{
    response<ResponseBody> res{ sc, req.version() };
    res.set(field::server, SERVER_SIGNATURE);
    res.set(field::content_type, ct);
    res.set(field::cache_control, hdCacheControl);
    res.set(field::pragma, hdPragma);
    res.content_length(body.size());
    res.keep_alive(req.keep_alive());
    if (sc != status::no_content) res.body() = body;
    res.prepare_payload();
    return res;
}

template<class RequestBody>
auto
make_resp_204(const request<RequestBody>& req)
{
	return make_resp<string_body>(req, "", "", status::no_content);
}

template<class RequestBody>
auto
make_resp_400(const request<RequestBody>& req, const std::string_view why)
{
	return make_resp<string_body>(req, "Bad request: " + std::string(why)
		, ctText, status::bad_request);
}

template<class RequestBody>
auto
make_resp_404(const request<RequestBody>& req)
{
	return make_resp<string_body>(req, "The resource '" + std::string(req.target()) + "' was not found."
			, ctText, status::not_found);
}

template<class RequestBody>
auto
make_resp_500(const request<RequestBody>& req, const std::string_view what)
{
	return make_resp<string_body>(req, "An error occurred: '" + std::string(what) + "'"
        	, ctText, status::internal_server_error);
}


// Return a reasonable mime type based on the extension of a file.
inline std::string_view
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

} // namespace btd

#endif // _HTTP_UTIL_HPP
