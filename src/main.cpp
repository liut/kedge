
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <utility>

#include <boost/asio/signal_set.hpp>

#include <libtorrent/config.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/version.hpp>

#include "const.hpp"
#include "handlers.hpp"
#include "listener.hpp"
#include "log.hpp"
#include "option.hpp"
#include "sheath.hpp"
#include "util.hpp"

#include "plog/Initializers/RollingFileInitializer.h"

using namespace btd;

int main(int argc, char* argv[])
{
    const std::string logMain(getLogsDir()+"/kedge-main.log");
    const std::string logAlert(getLogsDir()+"/kedge-alert.log");
    plog::init(plog::debug, logMain.c_str(), 1024*1024*32, 2); // Initialize the default logger instance.
    plog::init<AlertLog>(plog::debug, logAlert.c_str(), 1024*1024*64, 2); // Initialize the 2nd logger instance.

    option opt;
    if (!opt.init_from(argc, argv))
    {
        return EXIT_FAILURE;
    }

    lt::session_params params;
    const auto ses = std::make_shared<lt::session>(std::move(opt.params));

    const auto ctx = std::make_shared<sheath>(ses, opt.storeRoot, opt.movedRoot);

    std::thread ctx_start_loader([&ctx] {
        ctx->start();
    });

    const auto caller = std::make_shared<httpCaller>(ctx, opt.webuiRoot);

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

    std::cerr << "closing" << std::endl;

    return 0;
}
