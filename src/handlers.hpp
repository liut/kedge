#pragma once

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <boost/json/value.hpp>
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

static std::atomic_uint sync_ver = 0;

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

    // for save sync stats
    json::value prev_stats = json::value(nullptr);
    json::value curr_stats = json::value(nullptr);

public:

	// return string_body response
    const std::optional<http::response<string_body>>
    sbCall(http::request<string_body> const& req);

    http::response<string_body>
    handleSessionInfo(http::request<string_body> const& req);

    http::response<string_body>
    handleSessionStats(http::request<string_body> const& req);

    http::response<string_body>
    handleTorrents(http::request<string_body> const& req) ;

    http::response<string_body>
    handleTorrent(http::request<string_body> const& req, size_t const offset);

    http::response<string_body>
    handleSyncStats(http::request<string_body> const& req);

    json::value
    getSyncStats();

	void
	join(websocket_session* session);

	void
	leave(websocket_session* session);

	// Broadcast a message to all websocket client sessions
	void
	broadcast(std::string message);

    void
    closeWS();

    void
    doLoop();

    std::string const&
    ui_root() const noexcept
    {
        return ui_root_;
    }

	httpCaller(std::shared_ptr<sheath> const& shth, std::string ui_dir);

	// ~httpCaller();

};

} // namespace btd
