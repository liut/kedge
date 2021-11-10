#pragma once

#include <ctime>
#include <deque>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/config.hpp>
#include <boost/json/value.hpp>

namespace json = boost::json;

#include <libtorrent/alert_types.hpp>
#include <libtorrent/config.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_stats.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/span.hpp>

#include "session_stats.hpp"
#include "session_values.hpp"

// Forward declaration
// class websocket_session;
// class httpCaller;

namespace btd {
namespace fs = std::filesystem;

void
load_sess_params(std::string const& cd, lt::session_params& params);

json::object
torrent_status_to_json_obj(lt::torrent_status const& st);

void
tag_invoke( json::value_from_tag, json::value& jv, lt::torrent_status const& st);

using query_flags_t = std::uint16_t;

// Represents the shared server state
struct sheath : public std::enable_shared_from_this<sheath>
{
    friend class httpCaller;

    static constexpr query_flags_t query_basic = 1;
    static constexpr query_flags_t query_peers = 2;
    static constexpr query_flags_t query_files = 4;

    explicit
    sheath(std::shared_ptr<lt::session> const ses,
        std::string store_dir, std::string moved_dir);

    ~sheath()
    {
        end();
    }

    std::shared_ptr<lt::session> const&
    sess() const noexcept
    {
        return ses_;
    }

    json::value
    getSessionInfo() const;
    json::value
    getSessionStats() const;

    json::value
    get_torrents() const;
    json::value
    get_torrent(lt::sha1_hash const& ih, query_flags_t flags = query_basic) const;
    bool
    exists(lt::sha1_hash const& ih);
    bool
    drop_torrent(lt::sha1_hash const& ih, bool const with_data);

    std::string
    resume_file(lt::sha1_hash const& info_hash) const;

    bool
    set_peer(std::string const & addr);

    bool
    handle_alert(lt::alert* a);
    void
    save_all_resume();
    void
    start();
    void
    doLoop();
    void
    end();

    bool
    add_magnet(std::string const& uri);
    bool
    add_torrent(std::string const& filename);
    bool
    add_torrent(char const* buffer, int size, std::string const& save_path);
    json::value
    getSyncStats() const;

private:
    void
    load_resumes();

    void
    save_session();

    void
    pop_alerts();

    void
    set_torrent_params(lt::add_torrent_params& p);

    void
    scan_dir(std::string const& dir_path);

    void
    remove_torrent_with_handle(const lt::torrent_handle th);
    void
    set_all_torrents(const std::vector<lt::torrent_status> st);
    void
    update_json_stats();
    void
    on_torrent_finished(lt::torrent_handle const& h);

    std::shared_ptr<lt::session> const ses_;
    fs::path const dir_conf;
    fs::path const dir_moved;
    fs::path const dir_store;
    fs::path const dir_resumes;
    fs::path const dir_watches;
    fs::path const file_ses_state;
    lt::tcp::endpoint* peer_ = nullptr; // prepared peer ip:port

    sessionValues  svs = sessionValues();
    // all torrents
    std::unordered_map<lt::torrent_handle, lt::torrent_status> m_all_handles;

    std::deque<std::string>& events;

    // the number of times we've asked to save resume data
    // without having received a response (successful or failure)
    int num_outstanding_resume_data = 0;

    // int torrent_upload_limit = 0;
    // int torrent_download_limit = 0;
    int max_connections_per_torrent = 50;

    lt::time_duration refresh_delay = lt::milliseconds(500);
    lt::storage_mode_t allocation_mode = lt::storage_mode_sparse;
    bool enable_watch = true;
    lt::time_point next_dir_scan = lt::clock_type::now();


#ifndef TORRENT_DISABLE_DHT
    std::vector<lt::dht_lookup> dht_active_requests;
    std::vector<lt::dht_routing_bucket> dht_routing_table;
#endif


}; // sheath

} // namespace btd
