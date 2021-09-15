
#include <chrono>
#include <ctime>
#include <filesystem>

#include <libtorrent/bencode.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/session_handle.hpp>
#include <libtorrent/session_types.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_status.hpp>

#include "sheath.hpp"
#include "util.hpp"
// #include "http_ws_session.hpp"
#include "config.h"

namespace btd {
// using namespace std;
using namespace std::chrono;

bool
parse_endpoint(lt::tcp::endpoint & ep, std::string addr)
{
    if (!addr.empty())
    {
        char* port = (char*) strrchr((char*)addr.c_str(), ':');
        if (port != nullptr)
        {
            *port++ = 0;
            char const* ip = addr.c_str();
            int peer_port = atoi(port);
            lt::error_code ec;
            auto host = lt::address::from_string(ip, ec);
            if (ec)
            {
                fprintf(stderr, "invalid ip: '%s'\n", ip);
                return false;
            }
            if (peer_port > 0) {
                ep = lt::tcp::endpoint(host, std::uint16_t(peer_port));
                return true;
            }
        }
    }
    return false;
}

bool
prepare_dirs(std::string const & cd)
{
    fs::path cd_(cd);
    std::vector<fs::path> paths = {cd_
        , cd_ / RESUME_DIR
        , cd_ / WATCH_DIR
        , cd_ / CERT_DIR
    };
    for(fs::path &p : paths)
    {
        std::error_code ec;
        if (!fs::create_directory(p, ec) && ec.value() != 0)
        {
            // std::clog << "failed to create directory " << ec.message() << std::endl;
            std::fprintf(stderr, "failed to create directory: %s reason '%s'\n", p.c_str(), ec.message().c_str());
            return false;
        }
    }
    return true;
}

void
load_sess_params(std::string const& cd, lt::session_params& params)
{
    using lt::settings_pack;

    std::vector<char> in;
    if (load_file(path_cat(cd, SESS_FILE), in))
    {
        lt::error_code ec;
        lt::bdecode_node e = lt::bdecode(in, ec);
        lt::session::save_state_flags_t sft = lt::session::save_settings;
#ifndef TORRENT_DISABLE_DHT
        params.dht_settings.privacy_lookups = true;
        sft |= lt::session::save_dht_state;
#endif

        if (!ec) params = read_session_params(e, sft);
    }

    auto& settings = params.settings;
    settings.set_int(settings_pack::cache_size, -1);
    settings.set_int(settings_pack::choking_algorithm, settings_pack::rate_based_choker);

    // if (!settings.has_val(settings_pack::user_agent) || settings.get_str(settings_pack::user_agent) == "" )
    // {
        settings.set_str(settings_pack::user_agent, PROJECT_NAME "/" PROJECT_VER " lt/" LIBTORRENT_VERSION);
    // }

    settings.set_int(settings_pack::alert_mask
        , lt::alert_category::error
        | lt::alert_category::peer
        | lt::alert_category::port_mapping
        | lt::alert_category::storage
        | lt::alert_category::tracker
        | lt::alert_category::connect
        | lt::alert_category::status
        | lt::alert_category::ip_block
        | lt::alert_category::performance_warning
        | lt::alert_category::dht
        // | lt::alert_category::session_log
        // | lt::alert_category::torrent_log
        | lt::alert_category::incoming_request
        | lt::alert_category::dht_operation
        | lt::alert_category::port_mapping_log
        | lt::alert_category::file_progress);

    settings.set_bool(settings_pack::enable_upnp, false);
    settings.set_bool(settings_pack::enable_natpmp, false);
    settings.set_bool(settings_pack::enable_dht, false);
    settings.set_bool(settings_pack::enable_lsd, false);
    settings.set_bool(settings_pack::validate_https_trackers, false);
}


void
tag_invoke( json::value_from_tag, json::value& jv, sessionStats const& s )
{
    json::object obj;
    if (s.bytesRecv > 0) obj.emplace("bytesRecv", s.bytesRecv);
    if (s.bytesSent > 0) obj.emplace("bytesSent", s.bytesSent);
    if (s.bytesDataRecv > 0) obj.emplace("bytesDataRecv", s.bytesDataRecv);
    if (s.bytesDataSent > 0) obj.emplace("bytesDataSent", s.bytesDataSent);
    if (s.rateRecv > 0) obj.emplace("rateRecv", s.rateRecv);
    if (s.rateSent > 0) obj.emplace("rateSent", s.rateSent);
    if (s.bytesFailed > 0) obj.emplace("bytesFailed", s.bytesFailed);
    if (s.bytesQueued > 0) obj.emplace("bytesQueued", s.bytesQueued);
    if (s.bytesWasted > 0) obj.emplace("bytesWasted", s.bytesWasted);
    if (s.numChecking > 0) obj.emplace("numChecking", s.numChecking);
    if (s.numDownloading > 0) obj.emplace("numDownloading", s.numDownloading);
    if (s.numSeeding > 0) obj.emplace("numSeeding", s.numSeeding);
    if (s.numStopped > 0) obj.emplace("numStopped", s.numStopped);
    if (s.numQueued > 0) obj.emplace("numQueued", s.numQueued);
    if (s.numError > 0) obj.emplace("numError", s.numError);
    if (s.numPeersConnected > 0) obj.emplace("numPeersConnected", s.numPeersConnected);
    if (s.numPeersHalfOpen > 0) obj.emplace("numPeersHalfOpen", s.numPeersHalfOpen);
    if (s.limitUpQueue > 0) obj.emplace("limitUpQueue", s.limitUpQueue);
    if (s.limitDownQueue > 0) obj.emplace("limitDownQueue", s.limitDownQueue);
    if (s.hasIncoming) obj.emplace("hasIncoming", true);
    int activeCount = s.numChecking + s.numDownloading + s.numSeeding;
    int puasedCount = s.numQueued + s.numStopped;
    if (activeCount > 0) obj.emplace("activeCount", activeCount);
    if (puasedCount > 0) obj.emplace("puasedCount", puasedCount);
    obj.emplace("taskCount", activeCount + puasedCount);
    if (s.uptime > 0) obj.emplace("uptime", s.uptime);
    if (s.uptimeMs > 0) obj.emplace("uptimeMs", s.uptimeMs);
    jv = json::value(obj);
}

json::object
torrent_status_to_json_obj(lt::torrent_status const& st)
{
    constexpr auto hr_min = high_resolution_clock::time_point::min();
    json::object obj({
          { "added_time", st.added_time }
        , { "state", static_cast<int>(st.state) }
        , { "save_path", st.save_path } // empty in get_torrent_status
        , { "name", st.name }           // empty in get_torrent_status
        , { "info_hash", to_hex(st.info_hash) }
        , { "current_tracker", st.current_tracker  }
        , { "next_announce", duration_cast<seconds>(st.next_announce).count() }
        , { "active_duration", duration_cast<seconds>(st.active_duration).count() }
        , { "is_finished", st.is_finished }
        , { "progress", st.progress }
        , { "progress_ppm", st.progress_ppm }
    });
    if (st.completed_time > 0) obj.emplace("completed_time", st.completed_time);

    auto finished_duration = duration_cast<seconds>(st.finished_duration).count();
    if (finished_duration) obj.emplace("finished_duration", finished_duration);
    auto seeding_duration = duration_cast<seconds>(st.seeding_duration).count();
    if (seeding_duration) obj.emplace("seeding_duration", seeding_duration);

    if (st.total_download > 0) obj.emplace("total_download", st.total_download); // this session
    if (st.total_upload > 0) obj.emplace("total_upload", st.total_upload); // this session
    if (st.all_time_download > 0) obj.emplace("all_time_download", st.all_time_download);
    if (st.all_time_upload > 0) obj.emplace("all_time_upload", st.all_time_upload);
    if (st.total_payload_download > 0) obj.emplace("total_payload_download", st.total_payload_download); // this session
    if (st.total_payload_upload > 0) obj.emplace("total_payload_upload", st.total_payload_upload); // this session
    if (st.total_failed_bytes > 0) obj.emplace("total_failed_bytes", st.total_failed_bytes); // since last started
    if (st.total_redundant_bytes > 0) obj.emplace("total_redundant_bytes", st.total_redundant_bytes);
    if (st.total_done > 0) obj.emplace("total_done", st.total_done);
    if (st.total > 0) obj.emplace("total", st.total); // zero
    if (st.total_wanted_done > 0) obj.emplace("total_wanted_done", st.total_wanted_done);
    if (st.total_wanted > 0) obj.emplace("total_wanted", st.total_wanted);

    if (st.last_seen_complete > 0) obj.emplace("last_seen_complete", st.last_seen_complete);
    if (st.last_download > hr_min) obj.emplace("last_download", duration_cast<seconds>(st.last_download.time_since_epoch()).count());
    if (st.last_upload > hr_min) obj.emplace("last_upload", duration_cast<seconds>(st.last_upload.time_since_epoch()).count());

    if (st.download_rate > 0) obj.emplace("download_rate", st.download_rate);
    if (st.upload_rate > 0) obj.emplace("upload_rate", st.upload_rate);
    if (st.download_payload_rate > 0) obj.emplace("download_payload_rate", st.download_payload_rate);
    if (st.upload_payload_rate > 0) obj.emplace("upload_payload_rate", st.upload_payload_rate);
    if (st.num_seeds > 0) obj.emplace("num_seeds", st.num_seeds);
    if (st.num_peers > 0) obj.emplace("num_peers", st.num_peers);
    if (st.num_complete > 0) obj.emplace("num_complete", st.num_complete);
    if (st.num_incomplete > 0) obj.emplace("num_incomplete", st.num_incomplete);
    if (st.list_seeds > 0) obj.emplace("list_seeds", st.list_seeds);
    if (st.list_peers > 0) obj.emplace("list_peers", st.list_peers);
    if (st.connect_candidates > 0) obj.emplace("connect_candidates", st.connect_candidates);
    if (st.num_pieces > 0) obj.emplace("num_pieces", st.num_pieces); // zero
    if (st.distributed_full_copies > 0) obj.emplace("distributed_full_copies", st.distributed_full_copies);
    if (st.distributed_fraction > 0) obj.emplace("distributed_fraction", st.distributed_fraction);
    if (st.block_size > 0) obj.emplace("block_size", st.block_size);
    if (st.num_uploads > 0) obj.emplace("num_uploads", st.num_uploads);
    if (st.num_connections > 0) obj.emplace("num_connections", st.num_connections);
    if (st.moving_storage) obj.emplace("moving_storage", st.moving_storage);

    if (st.is_seeding) obj.emplace("is_seeding", st.is_seeding);
    if (st.has_metadata) obj.emplace("has_metadata", st.has_metadata);
    if (st.has_incoming) obj.emplace("has_incoming", st.has_incoming);

    if (st.errc)
    {
        obj.emplace("errc", st.errc.value());
    }
    return std::move(obj);
}

void
tag_invoke( json::value_from_tag, json::value& jv, lt::torrent_status const& st)
{
    jv = json::value(torrent_status_to_json_obj(st));
}


sheath::sheath(std::shared_ptr<lt::session> const ses, std::string store_dir)
    : ses_(ses)
    , dir_conf(getConfDir())
    , dir_store(std::move(store_dir))
    , dir_resumes(dir_conf / RESUME_DIR)
    , dir_watches(dir_conf / WATCH_DIR)
    , file_ses_state(dir_conf / SESS_FILE)
    , events(*(new std::deque<std::string>))
{
}

std::string
sheath::resume_file(lt::sha1_hash const& info_hash)
{
    auto file = dir_resumes / (to_hex(info_hash) + RESUME_EXT);
    return file.string();
}

void
sheath::load_resumes()
{
    using namespace std::chrono;
    // load resume files
    std::error_code ec;
    auto it = fs::directory_iterator(dir_resumes, ec);
    if (ec)
    {
        std::fprintf(stderr, "failed to list resume directory \"%s\": (%s : %d) %s\n"
            , dir_resumes.c_str(), ec.category().name(), ec.value(), ec.message().c_str());
        return;
    }

    std::fprintf(stderr, "resume directory: '%s' found\n", dir_resumes.c_str());
    for (auto const& e : it)
    {
        auto start = steady_clock::now();
        auto f_ = e.path();
        // only load resume files of the form <info-hash>.resume
        if (!is_resume_file(f_.filename().string())) {
            fprintf(stderr, " invalid resume name: '%s'\n", f_.filename().c_str());
            continue;
        }

        auto const fn_ = f_.string();

        std::vector<char> resume_data;
        if (!load_file(fn_, resume_data))
        {
            fprintf(stderr, "  failed to load resume file \"%s\"\n", fn_.c_str());
            continue;
        }
        lt::error_code ec_;
        lt::add_torrent_params atp = lt::read_resume_data(resume_data, ec_);
        if (ec_)
        {
            fprintf(stderr, "  failed to parse resume data \"%s\": %s\n"
                , fn_.c_str(), ec_.message().c_str());
            continue;
        }
        auto end = steady_clock::now();
        duration<double> elapsed = end-start;
        std::string name("");
        if (!atp.name.empty()) name = atp.name;
        else if(atp.ti) name = atp.ti->name();
        fprintf(stderr, "loaded resume %s(%s) a %s:%s done in %.2fs\n", f_.stem().c_str()
            , atp.name.c_str(), pptime(atp.added_time).c_str(), pptime(atp.completed_time).c_str(), elapsed.count());

        ses_->async_add_torrent(std::move(atp));
    }

}

void
sheath::start()
{
    load_resumes();
}

bool
sheath::set_peer(std::string const & addr)
{
    lt::tcp::endpoint ep;
    if (parse_endpoint(ep, addr))
    {
        peer_ = &ep;
        return true;
    }
    return false;
}

bool
sheath::handle_alert(lt::alert* a)
{
    using namespace lt;

    if (session_stats_alert* s = alert_cast<session_stats_alert>(a))
    {
        update_counters(s->counters(), duration_cast<microseconds>(s->timestamp().time_since_epoch()).count());
        return true;
    }

#ifndef TORRENT_DISABLE_DHT
    if (dht_stats_alert* p = alert_cast<dht_stats_alert>(a))
    {
        dht_active_requests = p->active_requests;
        dht_routing_table = p->routing_table;
        return true;
    }
#endif

    // don't log every peer we try to connect to or TODO: count it
    if (alert_cast<peer_connect_alert>(a)) return true;
    if (alert_cast<incoming_connection_alert>(a)) return true; // TODO: split logs

    if (peer_disconnected_alert* pd = alert_cast<peer_disconnected_alert>(a))
    {
        // ignore failures to connect and peers not responding with a
        // handshake. The peers that we successfully connect to and then
        // disconnect is more interesting.
        if (pd->op == operation_t::connect
            || pd->error == errors::timed_out_no_handshake)
            return true;
        // TODO: logs
    }

    if (metadata_received_alert* p = alert_cast<metadata_received_alert>(a))
    {
        torrent_handle h = p->handle;
        h.save_resume_data(torrent_handle::save_info_dict);
        ++num_outstanding_resume_data;
    }
    else if (add_torrent_alert* p = alert_cast<add_torrent_alert>(a))
    {
        if (p->error)
        {
            std::fprintf(stderr, "failed to add torrent: %s %s\n"
                , p->params.ti ? p->params.ti->name().c_str() : p->params.name.c_str()
                , p->error.message().c_str());
        }
        else
        {
            torrent_handle h = p->handle;

            h.save_resume_data(torrent_handle::save_info_dict | torrent_handle::only_if_modified);
            ++num_outstanding_resume_data;

            // if we have a peer specified, connect to it

            if (peer_ != nullptr)
            {
                h.connect_peer(*peer_);
            }
        }
    }
    else if (torrent_finished_alert* p = alert_cast<torrent_finished_alert>(a))
    {
        p->handle.set_max_connections(max_connections_per_torrent / 2); // ?

        // write resume data for the finished torrent
        // the alert handler for save_resume_data_alert
        // will save it to disk
        torrent_handle h = p->handle;
        h.save_resume_data(torrent_handle::save_info_dict);
        ++num_outstanding_resume_data;
        std::fprintf(stderr, "finished %s(%s)\n", to_hex(h.info_hash()).c_str(), p->torrent_name());
    }
    else if (save_resume_data_alert* p = alert_cast<save_resume_data_alert>(a))
    {
        std::fprintf(stderr, "saving resume %s(%s) a'%s' c'%s'\n", to_hex(p->params.info_hash).c_str(), p->params.name.c_str()
            , pptime(p->params.added_time).c_str(), pptime(p->params.completed_time).c_str());
        --num_outstanding_resume_data;
        auto const buf = lt::write_resume_data_buf(p->params);
        save_file(resume_file(p->params.info_hash), buf);
    }
    else if (save_resume_data_failed_alert* p = alert_cast<save_resume_data_failed_alert>(a))
    {
        --num_outstanding_resume_data;
        // don't print the error if it was just that we didn't need to save resume
        // data. Returning true means "handled" and not printed to the log
        return p->error == lt::errors::resume_data_not_modified;
    }
    else if (torrent_paused_alert* p = alert_cast<torrent_paused_alert>(a))
    {
        // write resume data for the finished torrent
        // the alert handler for save_resume_data_alert
        // will save it to disk
        torrent_handle h = p->handle;
        h.save_resume_data(torrent_handle::save_info_dict);
        ++num_outstanding_resume_data;
    }
    else if (state_update_alert* p = alert_cast<state_update_alert>(a))
    {
        // do nothing
        return true;
    }
    // TODO: more alerts

    return false;
}


void
print_alert(lt::alert const* a, std::string& str)
{
    str += "[";
    str += timestamp();
    str += "] ";
    str += a->message();

    log("[%s] %s %s\n", timestamp(), a->what(),  a->message().c_str());
}

void
sheath::pop_alerts()
{
    std::vector<lt::alert*> alerts;
    ses_->pop_alerts(&alerts);
    for (auto a : alerts)
    {
        if (handle_alert(a)) continue;

        // if we didn't handle the alert, print it to the log
        std::string event_string;
        print_alert(a, event_string);
        events.push_back(event_string);
        if (events.size() >= 20) events.pop_front();
    }
}

void
sheath::update_counters(lt::span<std::int64_t const> stats_counters, std::uint64_t const t)
{
    // only update the previous counters if there's been enough
    // time since it was last updated
    if (t - m_timestamp[1] > 2000000)
    {
        m_cnt[1].swap(m_cnt[0]);
        m_timestamp[1] = m_timestamp[0];
    }

    m_cnt[0].assign(stats_counters.begin(), stats_counters.end());
    m_timestamp[0] = t;
}


sessionStats
sheath::getSessionStats()
{
    struct sessionStats s;
    if (m_cnt[0].size() < m_queued_tracker_announces)
    {
        std::cerr << "ctx empty stats: " << m_cnt[0].size() << std::endl;
        return s;
    }
    s.numChecking = int(m_cnt[0][m_num_checking_idx]);
    s.numDownloading = int(m_cnt[0][m_num_downloading_idx]);
    s.numSeeding = int(m_cnt[0][m_num_seeding_idx]);
    s.numStopped = int(m_cnt[0][m_num_stopped_idx]);
    s.numQueued = int(m_cnt[0][m_num_queued_seeding_idx] + m_cnt[0][m_num_queued_download_idx]);
    s.numError = int(m_cnt[0][m_num_error_idx]);

    s.bytesRecv = std::uint64_t(m_cnt[0][m_recv_idx]);
    s.bytesSent = std::uint64_t(m_cnt[0][m_sent_idx]);
    s.bytesDataRecv = std::uint64_t(m_cnt[0][m_recv_data_idx]);
    s.bytesDataSent = std::uint64_t(m_cnt[0][m_send_data_idx]);

    if (m_cnt[1].size() >= m_queued_tracker_announces)
    {
        float const seconds = (m_timestamp[0] - m_timestamp[1]) / 1000000.f;
    int const download_rate = int((m_cnt[0][m_recv_idx] - m_cnt[1][m_recv_idx])
        / seconds);
    int const upload_rate = int((m_cnt[0][m_sent_idx] - m_cnt[1][m_sent_idx])
        / seconds);
    s.rateRecv = download_rate;
    s.rateSent = upload_rate;
    }

    s.bytesFailed = std::uint64_t(m_cnt[0][m_failed_bytes_idx]);
    s.bytesQueued = std::uint64_t(m_cnt[0][m_queued_bytes_idx]);
    s.bytesWasted = std::uint64_t(m_cnt[0][m_wasted_bytes_idx]);
    s.numPeersConnected = int(m_cnt[0][m_num_peers_connected_idx]);
    s.numPeersHalfOpen = int(m_cnt[0][m_num_peers_half_open_idx]);
    s.limitUpQueue = int(m_cnt[0][m_limiter_up_queue_idx]);
    s.limitDownQueue = int(m_cnt[0][m_limiter_down_queue_idx]);
    s.queuedTrackerAnnounces = int(m_cnt[0][m_queued_tracker_announces]);
    s.hasIncoming = int(m_cnt[0][m_has_incoming_idx]);
    s.uptime = uptime();
    s.uptimeMs = uptimeMs();
    return std::move(s);
}

json::value
sheath::get_stats_json()
{
    return json::value_from(getSessionStats());
}

void
sheath::save_session()
{
    if (!ses_->is_paused())
    {
        ses_->pause();
    }
    std::fprintf(stderr, "\nsaving session state\n");
    {
        lt::entry session_state;
        lt::session::save_state_flags_t sft = lt::session::save_settings;
#ifndef TORRENT_DISABLE_DHT
    // we're just saving the DHT state
        sft |= lt::session::save_dht_state;
#endif
        ses_->save_state(session_state, sft);

        std::vector<char> out;
        lt::bencode(std::back_inserter(out), session_state);
        save_file(file_ses_state.string() , out);
    }

}

void
sheath::set_torrent_params(lt::add_torrent_params& p)
{
    p.max_connections = max_connections_per_torrent;
    p.max_uploads = -1;
    // p.upload_limit = torrent_upload_limit;
    // p.download_limit = torrent_download_limit;

    // p.flags |= lt::torrent_flags::seed_mode;
    // if (disable_storage) p.storage = lt::disabled_storage_constructor;
    // p.flags |= lt::torrent_flags::share_mode;
    // auto dir = fs::path(store_dir_)
    if (p.save_path.empty()) p.save_path = dir_store.string();
    p.storage_mode = allocation_mode;
}

void
sheath::add_magnet(lt::string_view uri)
{
    lt::error_code ec;
    lt::add_torrent_params p = lt::parse_magnet_uri(uri.to_string(), ec);

    if (ec)
    {
        std::printf("invalid magnet link \"%s\": %s\n"
            , uri.to_string().c_str(), ec.message().c_str());
        return;
    }

    std::vector<char> resume_data;
    if (load_file(resume_file(p.info_hash), resume_data))
    {
        p = lt::read_resume_data(resume_data, ec);
        if (ec) std::printf("  failed to load resume data: %s\n", ec.message().c_str());
    }

    set_torrent_params(p);

    std::printf("adding magnet: %s\n", uri.to_string().c_str());
    ses_->async_add_torrent(std::move(p));
}

// return false on failure
bool
sheath::add_torrent(std::string filename)
{
    using lt::add_torrent_params;
    using lt::storage_mode_t;

    static int counter = 0;

    std::printf("[%d] %s\n", counter++, filename.c_str());

    lt::error_code ec;
    auto ti = std::make_shared<lt::torrent_info>(filename, ec);
    if (ec)
    {
        std::printf("failed to load torrent \"%s\": %s\n"
            , filename.c_str(), ec.message().c_str());
        return false;
    }

    add_torrent_params p;

    std::vector<char> resume_data;
    if (load_file(resume_file(ti->info_hash()), resume_data))
    {
        p = lt::read_resume_data(resume_data, ec);
        if (ec) std::printf("  failed to load resume data: %s\n", ec.message().c_str());
    }

    set_torrent_params(p);

    p.ti = ti;
    p.flags &= ~lt::torrent_flags::duplicate_is_error;
    ses_->async_add_torrent(std::move(p));
    return true;
}

bool
sheath::add_torrent(char const* buffer, int size, std::string const& save_path)
{
    using lt::add_torrent_params;
    using lt::storage_mode_t;

    lt::error_code ec;
    auto ti = std::make_shared<lt::torrent_info>(buffer, size, ec);
    if (ec)
    {
        std::fprintf(stderr, "failed to load torrent from buf(%d) : %s\n", size, ec.message().c_str());
        return false;
    }

    std::error_code ec_;
    if(!fs::create_directory(fs::path(save_path), ec_) && ec_.value() != 0) {
        std::fprintf(stderr, "failed to create directory(%s) : %s\n", save_path.c_str(), ec_.message().c_str());
        return false;
    }

    add_torrent_params p;

    std::vector<char> resume_data;
    if (load_file(resume_file(ti->info_hash()), resume_data))
    {
        p = lt::read_resume_data(resume_data, ec);
        if (ec) std::fprintf(stderr, "  failed to load resume data: %s\n", ec.message().c_str());
    }

    p.save_path = save_path;
    set_torrent_params(p);

    p.ti = ti;
    p.flags &= ~lt::torrent_flags::duplicate_is_error;
    ses_->async_add_torrent(std::move(p));
    return true;
}

void
sheath::scan_dir(std::string const& dir_path)
{
    using namespace lt;

    error_code ec;
    std::vector<std::string> ents = list_dir(dir_path
        , [](lt::string_view p) { return p.size() > 8 && p.substr(p.size() - 8) == ".torrent"; }, ec);
    if (ec)
    {
        std::fprintf(stderr, "failed to list directory: (%s : %d) %s\n"
            , ec.category().name(), ec.value(), ec.message().c_str());
        return;
    }

    for (auto const& e : ents)
    {
        std::string const file = path_cat(dir_path, e);

        // there's a new file in the monitor directory, load it up
        if (add_torrent(file))
        {
            if (::remove(file.c_str()) < 0)
            {
                std::fprintf(stderr, "failed to remove torrent file: \"%s\"\n"
                    , file.c_str());
            }
        }
    }
}

void
sheath::do_loop()
{
    using namespace std::chrono;
    ses_->post_torrent_updates();
    ses_->post_session_stats();
    ses_->post_dht_stats();

    std::this_thread::sleep_for(milliseconds(500));

    pop_alerts();

    lt::time_point const now = lt::clock_type::now();

    if (enable_watch && next_dir_scan < now)
    {
        scan_dir(dir_watches.string());
        next_dir_scan = now + seconds(WATCH_INTERVAL);
    }

}

void
sheath::save_all_resume()
{
    using namespace lt;
    // get all the torrent handles that we need to save resume data for
    std::vector<torrent_status> const temp = ses_->get_torrent_status(
        [](torrent_status const& st)
        {
            if (!st.handle.is_valid() || !st.has_metadata || !st.need_save_resume)
                return false;
            return true;
        }, {});

    int idx = 0;
    for (auto const& st : temp)
    {
        // save_resume_data will generate an alert when it's done
        st.handle.save_resume_data(torrent_handle::save_info_dict);
        ++num_outstanding_resume_data;
        ++idx;
        if ((idx % 32) == 0)
        {
            std::fprintf(stderr, "\r%d  ", num_outstanding_resume_data);
            pop_alerts();
        }
    }
    std::fprintf(stderr, "\nwaiting for resume data [%d]\n", num_outstanding_resume_data);

    while (num_outstanding_resume_data > 0)
    {
        alert const* a = ses_->wait_for_alert(seconds(S_WAIT_SAVE));
        if (a == nullptr) continue;
        pop_alerts();
    }
}

void
sheath::end()
{
    save_all_resume();
    save_session();
}

json::value
sheath::get_torrents()
{
    std::vector<lt::torrent_status> const torr = ses_->get_torrent_status(
                        [](lt::torrent_status const& st) { return true; }
                        , lt::torrent_handle::query_save_path | lt::torrent_handle::query_name);
    json::array arr;
    for(auto const& st: torr)
    {
        arr.emplace_back(torrent_status_to_json_obj(st));
    }
    return json::value(std::move(arr));
}

json::value
sheath::get_torrent(lt::sha1_hash const& ih, query_flags_t flags)
{
    auto th = ses_->find_torrent(ih);
    if (!th.is_valid())
    {
        return json::value(nullptr);
    }
    if (flags == query_basic)
    {
        auto st = th.status(lt::torrent_handle::query_name | lt::torrent_handle::query_save_path);

        json::object obj;
        obj = torrent_status_to_json_obj(st);
        return json::value(std::move(obj));
    }

    if (flags == query_peers)
    {
        json::array data;
        std::vector<lt::peer_info> peers;
        th.get_peer_info(peers);
        for (std::vector<lt::peer_info>::const_iterator i = peers.begin(); i != peers.end(); ++i)
        {
            lt::address const& addr = i->ip.address();
            json::object obj({
                 {"client", i->client}
                ,{"ip", addr.to_string()}
                ,{"ip", addr.to_string()}
                ,{"port", i->ip.port()}
                ,{"flag", static_cast<int>(i->flags)}
                ,{"source", static_cast<int>(i->source)}
                ,{"progress", i->progress_ppm}
                ,{"down_speed", i->down_speed}
                ,{"up_speed", i->up_speed}
                ,{"num_pieces", i->num_pieces}
            });
            if (i->flags & lt::peer_info::utp_socket) obj.emplace("uTP", true);
            data.emplace_back(obj);
        }
        return json::value(std::move(data));
    }

    if (flags == query_files)
    {
        std::vector<std::int64_t> file_progress;
        th.file_progress(file_progress);
        std::vector<lt::open_file_state> file_status = th.file_status();
        std::vector<lt::download_priority_t> file_prio = th.get_file_priorities();
        auto f = file_status.begin();
        std::shared_ptr<const lt::torrent_info> ti = th.torrent_file();

        json::array data;

        for (lt::file_index_t i(0); i < lt::file_index_t(ti->num_files()); ++i)
        {
            int const idx = static_cast<int>(i);
            int const progress = ti->files().file_size(i) > 0
                ? int(file_progress[idx] * 1000 / ti->files().file_size(i)) : 1000;
            assert(file_progress[idx] <= ti->files().file_size(i));

            bool const complete = file_progress[idx] == ti->files().file_size(i);

            json::object obj({
                 {"name", ti->files().file_name(i).to_string()}
                ,{"size", ti->files().file_size(i)}
                ,{"progress", progress}
                ,{"complete", complete}
                ,{"priority", static_cast<std::uint8_t>(file_prio[idx])}
            });

            if (f != file_status.end() && f->file_index == i)
            {
                if ((f->open_mode & lt::file_open_mode::rw_mask) == lt::file_open_mode::read_write) obj.emplace("state", "read/write");
                else if ((f->open_mode & lt::file_open_mode::rw_mask) == lt::file_open_mode::read_only) obj.emplace("state", "read");
                else if ((f->open_mode & lt::file_open_mode::rw_mask) == lt::file_open_mode::write_only) obj.emplace("state", "write");
                if (f->open_mode & lt::file_open_mode::random_access) obj.emplace("state", "random_access");
                if (f->open_mode & lt::file_open_mode::sparse) obj.emplace("state", "sparse");
                ++f;
            }
            data.emplace_back(obj);

        }
        return json::value(std::move(data));
    }
    return json::value(nullptr);
}
bool
sheath::exists(lt::sha1_hash const& ih)
{
    auto th = ses_->find_torrent(ih);
    if (th.is_valid())
    {
        return true;
    }
    return false;
}

bool
sheath::drop_torrent(lt::sha1_hash const& ih, bool const with_data)
{
    // also delete the resume file
    std::string const rpath = resume_file(ih);
    if (std::remove(rpath.c_str()) < 0)
        std::fprintf(stderr, "failed to delete resume file (\"%s\")\n", rpath.c_str());
    std::fprintf(stderr, "deleted resume file (\"%s\")\n", rpath.c_str());
    auto th = ses_->find_torrent(ih);
    if (th.is_valid())
    {
        lt::remove_flags_t flag;
        if (with_data)
        {
            flag = lt::session::delete_files;
        }
        ses_->remove_torrent(th, flag);
        return true;
    }
    return false;
}


} // namespace btd
