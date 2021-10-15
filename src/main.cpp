
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <utility>

#include <boost/asio/signal_set.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <libtorrent/config.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/version.hpp>


#include "config.h"
#include "handlers.hpp"
#include "listener.hpp"
#include "sheath.hpp"
#include "util.hpp"
#include "log.hpp"

using namespace btd;

std::string mapper(std::string env_var)
{
   // ensure the env_var is all caps
   std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);

   if (env_var == "LT_PEERID_PREFIX" || env_var == "TR_PEERID_PREFIX") return "peer-id";
   if (env_var == "KEDGE_STORE_ROOT") return "store-root";
   if (env_var == "DHT_BOOTSTRAP_NODES") return "dht-bootstrap-nodes";
   return "";
}

int main(int argc, char* argv[])
{

    set_logging(true);

    std::string peerID = "";
    std::string listens = "";
    std::string storeRoot = "";

    po::options_description config("configuration");
    config.add_options()
        ("help,h", "print usage message")
        ("listens,l", po::value<std::string>(&listens)->default_value("0.0.0.0:6881"), "listen_interfaces")
        ("store-root,d", po::value<std::string>(&storeRoot)->default_value(getStoreDir()), "store root, env: KEDGE_STORE_ROOT")
        ("peer-id", po::value<std::string>(&peerID)->default_value("-LT-"), "set prefix of fingerprint, env: LT_PEERID_PREFIX")
        ("dht-bootstrap-nodes", po::value<std::string>(), "a comma-separated list of Host port-pairs. env: DHT_BOOTSTRAP_NODES")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, config), vm);
    po::store(po::parse_environment(config, boost::function1<std::string, std::string>(mapper)), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << PROJECT_NAME << " " << PROJECT_VER << "\n";
        std::cout << " Flags and " << config << "\n";
        return 0;
    }

    using lt::session_handle;
    using lt::settings_pack;

    auto conf_dir = getConfDir();
    if (! prepare_dirs(conf_dir))
    {
        return EXIT_FAILURE;
    }
    lt::session_params params;
    load_sess_params(conf_dir, params);

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
    if (vm.count("store-root"))
    {
        LOG_DEBUG << "set storeRoot " << storeRoot;
    }
    if (vm.count("dht-bootstrap-nodes"))
    {
        params.settings.set_str(settings_pack::dht_bootstrap_nodes, vm["dht-bootstrap-nodes"].as<std::string>());
    }
    const auto ses = std::make_shared<lt::session>(std::move(params));

    const auto ctx = std::make_shared<sheath>(ses, storeRoot);

    std::thread ctx_start_loader([&ctx] {
        ctx->start();
    });

    const auto caller = std::make_shared<httpCaller>(ctx, getWebUI());

    // main: web server

    auto address = net::ip::make_address("127.0.0.1");
    auto port = static_cast<std::uint16_t>(16180);
    auto const threads = std::max<int>(1, std::thread::hardware_concurrency()/2 -1);

    std::cerr << "start http server on " << address << ":" << port << std::endl;

    // The io_context is required for all I/O
    net::io_context ioc;

    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        tcp::endpoint{address, port},
        caller)->run();

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](boost::system::error_code const&, int)
        {
            // Stop the io_context. This will cause run()
            // to return immediately, eventually destroying the
            // io_context and any remaining handlers in it.
            // ioc.stop();
            quit = true;
        });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> tv;
    tv.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        tv.emplace_back(
        [&ioc]
        {
            ioc.run();
        });

    std::thread shth_loader([&ctx, &ioc] {
        while (!quit)
        {
            ctx->doLoop();
        }
        ioc.stop();
    });

    std::thread caller_loader([&caller] {
        while (!quit)
        {
            caller->doLoop();
        }
    });

    std::cerr << "http server running" << std::endl;
    ioc.run(); // forever
    std::cerr << "http server ran ?" << std::endl;
    caller->closeWS();

    // Block until all the threads exit
    std::cerr << "thread joinning";
    // Block until all the threads exit
    for(auto& t : tv) {
        std::cerr << " .";
        t.join();
    }
    std::cerr << '\n';

    ctx_start_loader.join();
    shth_loader.join();
    caller_loader.join();
    std::cerr << "http server stopped" << std::endl;

    set_logging(false);
    std::cerr << "closing session" << std::endl;

    return 0;
}
