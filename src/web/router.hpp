#pragma once

#include <boost/beast/http.hpp>
#include <string>

#include "web/net.hpp"
#include "web/token.hpp"

namespace web {

template <class Handler, class Body, class Fields = http::fields>
class Router {
public:
    explicit Router(http::verb method)
            : method(method) {}

    [[nodiscard]] http::verb getMethod() const {
        return method;
    }

    Handler* findHandler(const http::request<Body, Fields>& request) {
        std::string s(request.target().data(), request.target().size());
        return rootToken.findMatch(std::move(s));
    }

    void addHandler(const std::string& path, Handler handler) {
        rootToken.addSubToken(path, handler);
    }

private:
    Token<Handler, char> rootToken;

    http::verb method;
};

}
