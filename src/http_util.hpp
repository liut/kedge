#ifndef _HTTP_UTIL_HPP
#define _HTTP_UTIL_HPP


#include <string_view>

#include <boost/json.hpp>
namespace json = boost::json;

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

} // namespace btd

#endif // _HTTP_UTIL_HPP
