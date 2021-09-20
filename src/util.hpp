#pragma once

#include <cstdint>
#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <system_error>
#include <filesystem>

#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/string_view.hpp>

#include "config.h"

namespace btd {

static std::atomic_bool quit(false);

namespace fs = std::filesystem;

const std::string DEFAULT_APP_NAME = "kedge";

void log(char const* fmt, ...);
bool is_logging();
void set_logging(bool enable);

char const*
timestamp();

bool
is_resume_file(std::string const& s);

bool
load_file(std::string const& filename, std::vector<char>& v, int limit = 8000000);

int
save_file(std::string const& filename, std::vector<char> const& v);

std::string const
path_cat(std::string_view const& base, std::string_view const& path);

bool
from_hex(lt::sha1_hash & ih, std::string const &s);

std::string
to_hex(lt::sha1_hash const& s);

std::time_t
now();

std::time_t
uptime();

std::int64_t
uptimeMs();

std::string const
getEnvStr(std::string const & key, std::string const & dft = "");

static std::string
getHomeDir();

std::string const
getConfDir(std::string const& app = APP_NAME);

std::string const
getLogsDir();

std::string const
getStoreDir();

std::string const
getWebUI();

std::int16_t
parse_port(std::string const& addr) noexcept;

std::string
pptime(std::time_t const & t);

std::uint32_t
randNum(std::uint32_t min = 1000, std::uint32_t max = 9999);

template<typename T>
std::string
n2hex(T i)
{
  std::stringstream stream;
  stream << std::setfill ('0') << std::setw(sizeof(T)*2)
         << std::hex << i;
  return stream.str();
}

template <typename Enumeration>
auto asValue(Enumeration const value)
    -> typename std::underlying_type<Enumeration>::type
{
    return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

} // namespace btd
