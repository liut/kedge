#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/websocket/error.hpp>

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
