#pragma once

#include <string>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <libtorrent/config.hpp>
#include <libtorrent/session.hpp>

#include "const.hpp"
#include "util.hpp"

namespace btd {

// Forward declaration
class sheath;

void
load_sess_params(std::string const& cd, lt::session_params& params);

struct option {

    std::string peerID    = "";
    std::string listens   = "";
    std::string movedRoot = "";
    std::string storeRoot = "";
    std::string webuiRoot = "";

	lt::session_params params;

    bool
    init_from(int argc, char* argv[]);

    std::shared_ptr<sheath>
    make_context() const;
};

std::string mapper(std::string env_var)
{
   // ensure the env_var is all caps
   std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);

   if (env_var == ENV_PEERID_PREFIX || env_var == "TR_PEERID_PREFIX") return "peer-id";
   if (env_var == ENV_BOOTSTRAP_NODES) return "dht-bootstrap-nodes";
   if (env_var == ENV_MOVED_ROOT) return "moved-root";
   if (env_var == ENV_STORE_ROOT) return "store-root";
   if (env_var == ENV_WEBUI_ROOT) return "webui-root";
   return "";
}

bool
option::init_from(int argc, char* argv[])
{
    po::options_description config("configuration");
    config.add_options()
        ("help,h", "print usage message")
        ("listens,l", po::value<std::string>(&listens)->default_value("0.0.0.0:6881"), "listen_interfaces")
        ("moved-root", po::value<std::string>(&movedRoot), "moved root, env: " ENV_MOVED_ROOT)
        ("store-root,d", po::value<std::string>(&storeRoot)->default_value(getStoreDir()), "store root, env: " ENV_STORE_ROOT)
        ("webui-root", po::value<std::string>(&webuiRoot)->default_value(getWebUI()), "web UI root, env: " ENV_WEBUI_ROOT)
        ("peer-id", po::value<std::string>(&peerID)->default_value("-LT-"), "set prefix of fingerprint, env: " ENV_PEERID_PREFIX)
        ("dht-bootstrap-nodes", po::value<std::string>()->default_value("dht.transmissionbt.com:6881"), "a comma-separated list of Host port-pairs. env: " ENV_BOOTSTRAP_NODES)
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, config), vm);
    po::store(po::parse_environment(config, boost::function1<std::string, std::string>(mapper)), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << PROJECT_NAME << " " << PROJECT_VER << "\n";
        std::cout << " Flags and " << config << "\n";
        return false;
    }

    auto conf_dir = getConfDir();
    if (!prepare_dirs(conf_dir)) return false;

    load_sess_params(conf_dir, params);

    using lt::session_handle;
    using lt::settings_pack;

    // std::uint16_t peerPort = parse_port(listens);
    if (vm.count("listens"))
    {
        LOG_DEBUG << "set listens " << listens << '|' << vm["listens"].as<std::string>();
        params.settings.set_str(settings_pack::listen_interfaces, vm["listens"].as<std::string>());
    }
    if (vm.count("peer-id"))
    {
        LOG_DEBUG << "set peerID " << peerID;
        params.settings.set_str(settings_pack::peer_fingerprint, peerID);
    }
    if (vm.count("moved-root"))
    {
        LOG_DEBUG << "set moved root " << movedRoot;
    }
    if (vm.count("store-root"))
    {
        LOG_DEBUG << "set store root " << storeRoot;
    }
    if (vm.count("dht-bootstrap-nodes"))
    {
        auto nodes = vm["dht-bootstrap-nodes"].as<std::string>();
        LOG_DEBUG << "set dht-bootstrap-nodes " << nodes;
        params.settings.set_str(settings_pack::dht_bootstrap_nodes, nodes);
    }

    return true;
}

std::shared_ptr<sheath>
option::make_context() const
{
    const auto ses = std::make_shared<lt::session>(std::move(params));
    const auto ctx = std::make_shared<sheath>(ses, storeRoot, movedRoot);
    return ctx;
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

} // namespace btd
