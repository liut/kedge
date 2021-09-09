

#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <boost/json.hpp>
namespace json = boost::json;

#include <libtorrent/config.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/version.hpp>

#include "handlers.hpp"
#include "http_websocket.hpp"

namespace btd {

httpCaller::
httpCaller(std::shared_ptr<sheath> const& shth,
	std::string ui_dir)
	 : shth_(shth)
	 , ui_root_(ui_dir)
{}


const std::optional<http::response<string_body>>
httpCaller::
sbCall(http::request<string_body> const& req)
{
	if (req.method() == verb::get && req.target() == "/sess"sv) return handleSession(req);
	if (req.method() == verb::get && req.target() == "/stats"sv) return handleStats(req);
	if (req.target() == "/torrents"sv) return handleTorrents(req);

    if (req.target().find("/torrent/") == 0) return handleTorrent(req);

	// TODO: more routes
	return std::nullopt;
}

http::response<string_body>
httpCaller::
handleSession(http::request<string_body> const& req)
{
	auto const settings = shth_->sess()->get_settings();
	auto const peerID = settings.get_str(lt::settings_pack::peer_fingerprint);
	auto const peerPort = parse_port(settings.get_str(lt::settings_pack::listen_interfaces));
	const json::value jv= {{"peerID", peerID}, {"peerPort", peerPort}
        , {"uptime", uptime()}, {"uptimeMs", uptimeMs()}, {"version", lt::version()}};
    return make_resp<string_body>(req, json::serialize(jv), ctJSON);
}

http::response<string_body>
httpCaller::
handleStats(http::request<string_body> const& req)
{
	return make_resp<string_body>(req, json::serialize(shth_->get_stats_json()), ctJSON);
}

http::response<string_body>
httpCaller::
handleTorrents(http::request<string_body> const& req) // get or post
{
	if (req.method() == verb::get)
	{
		return make_resp<string_body>(req, json::serialize(shth_->get_torrents()), ctJSON);
	}

	if (req.method() == verb::post)
	{
		std::clog << " post clen " << req.find(http::field::content_length)->value() << " bytes\n";
		auto savePath = req.find("x-save-path");
		if (savePath == req.end())
		{
			return make_resp_400(req, "miss save-path");
		}
		const char* buf = req.body().data();
		const int size = req.body().size();
		const std::string dir(savePath->value());
		std::clog << "post metainfo " << size << " bytes with save-path: '" << dir << "'\n";
		if (shth_->add_torrent(buf, size, dir))
		{
			return make_resp_204(req);
		}
		return make_resp_500(req, "failed to add");
	}

	return make_resp_400(req, "Unsupported method");
}


http::response<string_body>
httpCaller::
handleTorrent(http::request<string_body> const& req) // get or post or delete torrent
{
	// /torrent/{info_hash}/{act}?
	const std::string s(req.target().data(), req.target().size());
	const int ps = 40 + 9;
	if (s.size() < ps) return make_resp_404(req);

	lt::sha1_hash ih;
	if (!from_hex(ih, s.substr(9, 40)))
	{
		return make_resp_400(req, "invalid hash string");
	}
	if (req.method() == verb::head)
	{
		if (shth_->exists(ih))
		{
			return make_resp_204(req);
		}
		return make_resp_404(req);
	}
	std::string act = "";
	if (s.size() > ps + 1)
	{
		act = s.substr(ps + 1);
	}
	if (req.method() == verb::get)
	{
		auto flag = sheath::query_basic;
		if ("peers" == act) flag = sheath::query_peers;
		else if ("files" == act) flag = sheath::query_files;
		auto jv = shth_->get_torrent(ih, flag);
		if (jv.is_null()) { return make_resp_404(req); }
		return make_resp<string_body>(req, json::serialize(jv), ctJSON);
	}
	if (req.method() == verb::delete_)
	{
		shth_->drop_torrent(ih, ("yes" == act || "with_data" == act));
		return make_resp_204(req);
	}

	return make_resp_400(req, "Unsupported method");
}

void
httpCaller::
join(websocket_session* session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.insert(session);
}

void
httpCaller::
leave(websocket_session* session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session);
}

// Broadcast a message to all websocket client sessions
void
httpCaller::
send(std::string message)
{
    // Put the message in a shared pointer so we can re-use it for each client
    auto const ss = std::make_shared<std::string const>(std::move(message));

    // Make a local list of all the weak pointers representing
    // the sessions, so we can do the actual sending without
    // holding the mutex:
    std::vector<std::weak_ptr<websocket_session>> vws;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        vws.reserve(sessions_.size());
        for(auto p : sessions_)
            vws.emplace_back(p->weak_from_this());
    }

    // For each session in our local list, try to acquire a strong
    // pointer. If successful, then send the message on that session.
    for(auto const& wp : vws)
        if(auto sp = wp.lock())
            sp->send(ss);
}



} // namespace btd
