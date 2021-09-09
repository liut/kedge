#pragma once

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

#include "sheath.hpp"
#include "net.hpp"
#include "http_util.hpp"
#include "util.hpp"

namespace btd {

// Forward declaration
class websocket_session;

class httpCaller : public std::enable_shared_from_this<httpCaller>
{
	// sheath of libtorrent session
    std::shared_ptr<sheath> shth_;

    // direcotry of web ui
    std::string const ui_root_;

    // This mutex synchronizes all access to sessions_
    std::mutex mutex_;

    // Keep a list of all the connected clients
    std::unordered_set<websocket_session*> sessions_;

public:

	// return string_body response
	template<class RequestBody>
    const std::optional<http::response<string_body>>
    call_sb(http::request<RequestBody> const& req)
    {
    	if (req.method() == verb::get && req.target() == "/sess"sv) return getSession(req);
    	if (req.method() == verb::get && req.target() == "/stats"sv) return getStats(req);
    	if (req.target() == "/torrents"sv) return handleTorrents(req);

	    if (req.target().find("/torrent/") == 0) return handleTorrent(req);

    	// TODO: more routes
    	return std::nullopt;
    }

    http::response<string_body>
    getSession(http::request<string_body> const& req)
    {
    	auto const settings = shth_->sess()->get_settings();
    	auto const peerID = settings.get_str(lt::settings_pack::peer_fingerprint);
    	auto const peerPort = parse_port(settings.get_str(lt::settings_pack::listen_interfaces));
    	const json::value jv= {{"peerID", peerID}, {"peerPort", peerPort}
            , {"uptime", uptime()}, {"uptimeMs", uptimeMs()}, {"version", lt::version()}};
        return make_resp<string_body>(req, json::serialize(jv), ctJSON);
    }

    http::response<string_body>
    getStats(http::request<string_body> const& req)
    {
    	return make_resp<string_body>(req, json::serialize(shth_->get_stats_json()), ctJSON);
    }

    http::response<string_body>
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
	join(websocket_session* session)
	{
	    std::lock_guard<std::mutex> lock(mutex_);
	    sessions_.insert(session);
	}

	void
	leave(websocket_session* session)
	{
	    std::lock_guard<std::mutex> lock(mutex_);
	    sessions_.erase(session);
	}


    std::string const&
    ui_root() const noexcept
    {
        return ui_root_;
    }

	httpCaller(std::shared_ptr<sheath> const& shth, std::string ui_dir)
	 : shth_(shth)
	 , ui_root_(ui_dir)
	 {}
	// ~httpCaller();

};

} // namespace btd
