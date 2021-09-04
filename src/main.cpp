
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <utility>

#include <boost/json.hpp>
namespace json = boost::json;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <libtorrent/config.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/version.hpp>

#include "sheath.hpp"
#include "util.hpp"
#include "web/server.hpp"
#include "config.h"

namespace {
  volatile std::sig_atomic_t gSignalStatus;
}

using namespace http;
using namespace web;

using namespace btd;

const std::string CT_JSON = "application/json";
const std::string CT_TEXT = "text/plain";

static std::atomic_bool quit(false);

void signal_handler(int signal)
{
    gSignalStatus = signal;
    std::cerr << "got signal " << signal << std::endl;
    // make the main loop terminate
    if (signal == SIGINT || signal == SIGTERM) {
        quit = true;
    }
}

std::string mapper(std::string env_var)
{
   // ensure the env_var is all caps
   std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);

   if (env_var == "LT_PEERID_PREFIX" || env_var == "TR_PEERID_PREFIX") return "peer-id";
   if (env_var == "DHT_BOOTSTRAP_NODES") return "dht-bootstrap-nodes";
   return "";
}

int main(int argc, char* argv[])
{

    set_logging(true);

    std::string peerID = "";
    std::string listens = "";

    po::options_description config("Configuration");
    config.add_options()
        ("help,h", "print usage message")
        ("listens", po::value<std::string>(&listens)->default_value("0.0.0.0:6881"), "listen_interfaces")
        ("peer-id", po::value<std::string>(&peerID)->default_value("-LT-"), "set prefix of fingerprint, env: LT_PEERID_PREFIX")
        ("dht-bootstrap-nodes", po::value<std::string>(), "a comma-separated list of IP port-pairs. env: DHT_BOOTSTRAP_NODES")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, config), vm);
    po::store(po::parse_environment(config, boost::function1<std::string, std::string>(mapper)), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << PROJECT_NAME << " " << PROJECT_VER << "\n";
        std::cout << config << "\n";
        return 0;
    }

    using lt::session_handle;
    using lt::settings_pack;

    auto conf_dir = getConfDir("kedge");
    if (! prepare_dirs(conf_dir))
    {
        return EXIT_FAILURE;
    }
    lt::session_params params;
    load_sess_params(conf_dir, params);

    std::uint16_t peerPort = parse_port(listens);
    if (vm.count("listens"))
    {
        std::clog << "set listens " << listens << '|' << vm["listens"].as<std::string>() << '\n';
        params.settings.set_str(settings_pack::listen_interfaces, vm["listens"].as<std::string>());
    }
    if (vm.count("peer-id"))
    {
        std::clog << "set peerID " << peerID << '\n';
        params.settings.set_str(settings_pack::peer_fingerprint, peerID);
    }
    if (vm.count("dht-bootstrap-nodes"))
    {
        params.settings.set_str(settings_pack::dht_bootstrap_nodes, vm["dht-bootstrap-nodes"].as<std::string>());
    }
    const auto ses = std::make_shared<lt::session>(std::move(params));

    const auto ctx = std::make_shared<sheath>(ses, "./store");

    std::thread ctx_start_loader([&ctx] {
        ctx->start();
    });

    // main: web server

    auto address = net::ip::make_address("127.0.0.1");
    auto port = static_cast<std::uint16_t>(16180);
    auto const threads = std::max<int>(1, std::thread::hardware_concurrency()/2 -1);
    auto httpServer = std::make_shared<HttpServer<string_body>>(threads);

    std::cerr << "start http server on " << address << ":" << port << std::endl;
    httpServer->start(address, port, threads);

    httpServer->addRoute(verb::get, "/", [](auto req) {
        return make_resp_204<string_body>(req);
    });
    httpServer->addRoute(verb::get, "/sess", [&peerID, &peerPort](auto req) {
        const json::value jv= {{"peerID", peerID}, {"peerPort", peerPort}
            , {"uptime", uptime()}, {"uptimeMs", uptimeMs()}, {"version", lt::version()}};
        return make_resp<string_body>(req, json::serialize(jv), CT_JSON);
    });
    httpServer->addRoute(verb::get, "/stats", [&ctx](auto req) {
        return make_resp<string_body>(req, json::serialize(ctx->get_stats_json()), CT_JSON);
    });
    httpServer->addRoute(verb::get, "/torrents", [&ctx](auto req) {
        return make_resp<string_body>(req, json::serialize(ctx->get_torrents()), CT_JSON);
    });
    httpServer->addRoute(verb::post, "/torrents", [&ctx](auto req) {
        std::clog << " post clen " << req.find(http::field::content_length)->value() << " bytes\n";
        auto savePath = req.find("x-save-path");
        if (savePath == req.end())
        {
            return make_resp<string_body>(req, "miss save-path", CT_TEXT, status::bad_request);
        }
        const char* buf = req.body().data();
        const int size = req.body().size();
        const std::string dir(savePath->value());
        std::clog << "post metainfo " << size << " bytes with save-path: '" << dir << "'\n";
        if (ctx->add_torrent(buf, size, dir))
        {
            return make_resp_204<string_body>(req);
        }
        return make_resp<string_body>(req, "failed to add", CT_TEXT, status::service_unavailable);

    });
    httpServer->setNotFoundHandler([&ctx](auto && req) {
        const std::string s(req.target().data(), req.target().size());
        const int ps = 40 + 9;
        if (s.size() >= ps && s.substr(0, 9) == "/torrent/") // /torrent/{info_hash}/{act}?
        {
            lt::sha1_hash ih;
            if (!from_hex(ih, s.substr(9, 40)))
            {
                return make_resp<string_body>(req, "invalid hash string", CT_TEXT, status::bad_request);
            }
            if (req.method() == verb::head)
            {
                if (ctx->exists(ih))
                {
                    return make_resp_204<string_body>(req);
                }
                return make_resp<string_body>(req, "not found", CT_TEXT, status::not_found);
            }
            std::string act = "";
            if (s.size() > ps+1)
            {
                act = s.substr(ps+1);
            }
            if (req.method() == verb::get)
            {
                auto flag = sheath::query_basic;
                if ("peers" == act) flag = sheath::query_peers;
                else if ("files" == act) flag = sheath::query_files;
                auto jv = ctx->get_torrent(ih, flag);
                if (jv.is_null())
                {
                    return make_resp<string_body>(req, "not found", CT_TEXT, status::not_found);
                }
                return make_resp<string_body>(req, json::serialize(jv), CT_JSON);
            }
            if (req.method() == verb::delete_)
            {
                ctx->drop_torrent(ih, ("yes" == act || "with_data" == act));
                return make_resp_204<string_body>(req);
            }


        } // torrent
        return make_resp<string_body>(req, "not found", CT_TEXT, status::not_found);
    });
    // TODO: routes

#ifndef _WIN32
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#endif
    std::thread main_loader([&ctx, &httpServer] {
        while (!quit)
        {
            ctx->do_loop();
        }
        httpServer->stop();
    });

    std::cerr << "http server running" << std::endl;
    httpServer->getIoContext().run();
    std::cerr << "http server ran ?" << std::endl;

    // Block until all the threads exit
    std::cerr << "thread joinning";
    for (auto & thread : httpServer->getIoThreads()) {
        std::cerr << " .";
        thread.join();
    }
    std::cerr << '\n';

    ctx_start_loader.join();
    main_loader.join();
    std::cerr << "http server stopped" << std::endl;

    ctx->end();

    set_logging(false);
    std::cerr << "closing session" << std::endl;

    return 0;
}
