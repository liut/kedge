#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <system_error>
#include <filesystem>

#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/string_view.hpp>

namespace btd {

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

std::vector<std::string>
list_dir(std::string path, bool (*filter_fun)(lt::string_view), lt::error_code& ec);

std::string
path_cat(std::string const& base, std::string const& path);

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
getConfDir(std::string const& app);

std::string const
getLogsDir();

std::int16_t
parse_port(std::string const& addr) noexcept;

std::string
pptime(std::time_t const & t);

} // namespace btd
