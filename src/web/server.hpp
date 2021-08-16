#pragma once

#include <boost/beast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread>
#include <vector>

#include "web/router.hpp"
#include "web/listener.hpp"

namespace web {

using namespace http;

template<class Body>
response<Body>
make_resp(const request<Body>& req, typename Body::value_type body, beast::string_view ct, status sc = status::ok)
{
    response<string_body> res{ sc, req.version() };
    res.set(field::server, BOOST_BEAST_VERSION_STRING);
    res.set(field::content_type, ct);
    res.keep_alive(req.keep_alive());
    res.body() = body;
    res.prepare_payload();
    return res;
}

template<class Body>
response<Body>
make_resp_204(const request<Body>& req)
{
    response<string_body> res{ status::no_content, req.version() };
    res.set(field::server, BOOST_BEAST_VERSION_STRING);
    // res.set(field::content_type, ct);
    // res.keep_alive(req.keep_alive());
    // res.body() = body;
    res.prepare_payload();
    return res;
}

template <class Body>
class HttpServer : public std::enable_shared_from_this<HttpServer<Body>> {
public:
    typedef std::function<http::response<Body>(http::request<Body>&& request)> Handler;

    explicit HttpServer(uint32_t threads = 1) : ioc(threads) {
        routers.reserve((unsigned)http::verb::unlink);
        for (auto i = 0U; i < routers.capacity(); ++i) {
            routers.emplace_back((http::verb)i);
        }
    }
    ~HttpServer() {
        io_threads.clear();
    }

    void start(const net::ip::address& address, uint16_t port, uint32_t threadCount = 1) {
        listener = std::make_shared<Listener<Body>>(ioc, tcp::endpoint{address, port}, this->shared_from_this());
        listener->run();
        for (auto i = 0U; i < threadCount; ++i) {
            io_threads.emplace_back([&] {
                this->getIoContext().run();
            });
        }
    }

    void stop() {
        listener->stop();
        ioc.stop();
    }

    void addRoute(http::verb verb, const std::string &path, Handler handler) {
        routers[(unsigned)verb].addHandler(path, handler);
    }

    void setNotFoundHandler(Handler handler) {
        notFoundHandler = handler;
    }

    net::io_context& getIoContext() {
        return ioc;
    }

    std::vector<std::thread>& getIoThreads() {
        return io_threads;
    }

    http::response<Body> requestHandler(http::request<Body> && request) {
        auto methodIdx = (unsigned)request.method();
        Handler *handler = nullptr;
        if (methodIdx < routers.size()) {
            handler = routers[methodIdx].findHandler(request);
        }

        if (!handler) {
            handler = &notFoundHandler;
        }

        return (*handler)(std::move(request));
    }

private:
    Handler notFoundHandler;
    net::io_context ioc;
    std::vector<std::thread> io_threads;
    std::shared_ptr<Listener<Body>> listener;
    std::vector<Router<Handler, Body>> routers;
};

}
