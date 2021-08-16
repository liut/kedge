#pragma once


#include <ctime>
#include <filesystem>
#include <iostream>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
// #include <unordered_set>
#include <vector>

#include <boost/config.hpp>
#include <boost/json.hpp>

namespace json = boost::json;

#include <libtorrent/config.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_stats.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/span.hpp>

#include "session_values.hpp"

// Forward declaration
// class websocket_session;

namespace btd {
using namespace std::literals;
namespace fs = std::filesystem;

const std::string SESS_FILE = ".ses_state"s;
const std::string RESUME_DIR = ".resume"s;
const std::string RESUME_EXT = ".resume"s;
const std::string WATCH_DIR = "watching"s;
const std::string CERT_DIR = "certificates"s;
const int WATCH_INTERVAL = 2; // seconds
const int S_WAIT_SAVE = 6; // seconds


bool
prepare_dirs(std::string const & cd);

void
load_sess_params(std::string const& cd, lt::session_params& params);

std::string
resume_file(std::string const& cd, lt::sha1_hash const& info_hash);


struct stats
{
    int64_t bytesRecv;
    int64_t bytesSent;
    int64_t bytesDataRecv;
    int64_t bytesDataSent;
    int64_t rateRecv;
    int64_t rateSent;
    int64_t bytesFailed;
    int64_t bytesWasted;
    int64_t bytesQueued;
    int numChecking;
    int numDownloading;
    int numSeeding;
    int numStopped;
    int numQueued;
    int numError;
    int numPeersConnected;
    int numPeersHalfOpen;
    int limitUpQueue;
    int limitDownQueue;
    int queuedTrackerAnnounces;
    bool hasIncoming;
    time_t uptime;
    uint64_t uptimeMs;
};

void
tag_invoke( json::value_from_tag, json::value& jv, stats const& s );

json::object
torrent_status_to_json_obj(lt::torrent_status const& st);

void
tag_invoke( json::value_from_tag, json::value& jv, lt::torrent_status const& st);

using query_flags_t = std::uint16_t;

// Represents the shared server state
struct context : sessionValues
{

    static constexpr query_flags_t query_basic = 1;
    static constexpr query_flags_t query_peers = 2;
    static constexpr query_flags_t query_files = 4;

    explicit
    context(std::shared_ptr<lt::session> const ses, std::string store_dir);

    std::shared_ptr<lt::session> const&
    sess() const noexcept
    {
        return ses_;
    }

    stats
    get_stats();

    json::value
    get_stats_json();

    json::value
    get_torrents();
    json::value
    get_torrent(lt::sha1_hash const& ih, query_flags_t flags = query_basic);
    bool
    exists(lt::sha1_hash const& ih);
    bool
    drop_torrent(lt::sha1_hash const& ih, bool const with_data);

    void
    update_counters(lt::span<std::int64_t const> stats_counters, std::uint64_t t);

    std::string
    resume_file(lt::sha1_hash const& info_hash);

    bool
    set_peer(std::string const & addr);

    bool
    handle_alert(lt::alert* a);
    void
    save_all_resume();
    void
    start();
    void
    do_loop();
    void
    end();

    void
    add_magnet(lt::string_view uri);
    bool
    add_torrent(std::string filename);
    bool
    add_torrent(char const* buffer, int size, std::string const& save_path);

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

    std::shared_ptr<lt::session> const ses_;
    fs::path const dir_conf;
    fs::path const dir_store;
    fs::path const dir_resumes;
    fs::path const dir_watches;
    fs::path const file_ses_state;
    lt::tcp::endpoint* peer_ = nullptr; // prepared peer ip:port

    // This mutex synchronizes all access to sessions_
    std::mutex mutex_;

    // Keep a list of all the connected clients
    // std::unordered_set<websocket_session*> sessions_;

    std::deque<std::string>& events;

    // there are two sets of counters. the current one and the last one. This
    // is used to calculate rates
    std::vector<std::int64_t> m_cnt[2];

    // the timestamps of the counters in m_cnt[0] and m_cnt[1]
    // respectively. The timestamps are microseconds since session start
    std::uint64_t m_timestamp[2];

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


}; // context

} // namespace btd
