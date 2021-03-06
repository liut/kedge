
#include <chrono>    // time and clock
#include <cinttypes> // for PRId64 et.al.
#include <cstdarg>   // va_start, va_end
#include <cstdio>    // FILE
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string.h> // strdup
#include <thread>
#include <utility>

#ifndef _WIN32
#include <pwd.h>
#endif

#include <libtorrent/config.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/string_view.hpp>
#include <libtorrent/time.hpp>

#include "log.hpp"
#include "util.hpp"

namespace btd {
using namespace std::chrono;
static std::time_t start_up  = std::time(0);
static time_point start_up_c = system_clock::now();

std::time_t
now()
{
    return std::time(0);
}

std::time_t
uptime()
{
    return now() - start_up;
}

std::int64_t
uptimeMs()
{
    auto cur_c  = system_clock::now();
    auto int_ms = duration_cast<milliseconds>(cur_c - start_up_c);
    return int_ms.count();
}

char const *
timestamp()
{
    time_t t     = std::time(nullptr);
    tm *timeinfo = std::localtime(&t);
    static char str[200];
    std::strftime(str, 200, "%m%d %X", timeinfo);
    return str;
}

bool
is_resume_file(std::string const &s)
{
    static std::string const hex_digit = "0123456789abcdef";
    if (s.size() != 40 + 7) return false;
    if (s.substr(40) != ".resume") return false;
    for (char const c : s.substr(0, 40)) {
        if (hex_digit.find(c) == std::string::npos) return false;
    }
    return true;
}

bool
load_file(std::string const &filename, std::vector<char> &v, int limit)
{
    std::fstream f(filename, std::ios_base::in | std::ios_base::binary);
    f.seekg(0, std::ios_base::end);
    auto const s = f.tellg();
    if (s > limit || s < 0) return false;
    f.seekg(0, std::ios_base::beg);
    v.resize(static_cast<std::size_t>(s));
    if (s == std::fstream::pos_type(0)) return !f.fail();
    f.read(v.data(), v.size());
    return !f.fail();
}

int
save_file(std::string const &filename, std::vector<char> const &v)
{
    std::fstream f(filename, std::ios_base::trunc | std::ios_base::out |
                                 std::ios_base::binary);
    f.write(v.data(), v.size());
    return !f.fail();
}

std::string const
path_cat(std::string_view const &base, std::string_view const &path)
{
    auto result = fs::path(base) += path;
    return result.string();
}

bool
from_hex(lt::sha1_hash &ih, std::string const &s)
{
    if (s.size() != 40) { return false; }
    std::stringstream hash(s.c_str());
    hash >> ih;
    if (hash.fail()) { return false; }
    return true;
}

std::string
to_hex(lt::sha1_hash const &ih)
{
    std::stringstream ret;
    ret << ih;
    return ret.str();
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
            LOG_ERROR << "failed to create directory " << p << " reason " << ec.message();
            return false;
        }
    }
    return true;
}

std::string const
getEnvStr(std::string const &key, std::string const &dft)
{
    char *val = std::getenv(key.c_str());
    return val == nullptr ? dft : std::string(val);
}

static std::string
getHomeDir()
{
    static char *home = nullptr;
    if (home == nullptr) {
        home = std::getenv("HOME");
        if (home == nullptr) {
            struct passwd *pw = getpwuid(getuid());
            if (pw != NULL) { home = strdup(pw->pw_dir); }
            endpwent();
        }
        if (home == nullptr) { home = strdup(""); }
    }

    return home;
}

std::string const
getConfDir(std::string const &app)
{
    // TODO: _WIN32
#ifdef __APPLE__
    auto dir = fs::path(getHomeDir()) / "Library" / "Application Support" / app;
#else
    auto xcfg = fs::path(getHomeDir()) / ".config";
    auto dir  = fs::path(getEnvStr("XDG_CONFIG_HOME", xcfg.string())) / app;
#endif
    return dir.string();
}

std::string const
getLogsDir()
{
    auto dir = fs::path(getHomeDir()) / "logs";
    return dir.string();
}

std::string const
getStoreDir()
{
    auto dir = fs::path(getHomeDir()) / "Downloads";
    return dir.string();
}

std::string const
getWebUI()
{
    auto dir = fs::path(getConfDir()) / "webui";
    return dir.string();
}

std::int16_t
parse_port(std::string const &addr) noexcept
{
    if (addr.empty()) { return 0; }
    char *port = (char *)strrchr((char *)addr.c_str(), ':');
    if (port != nullptr) {
        port++;
        return std::uint16_t(atoi(port));
    }

    // auto pos = addr.rfind(':');
    // if (pos == std::string::npos) return 0;
    // std::string port = addr.substr(pos);
    // if (port.empty()) return 0;
    // try {
    //     int pn = std::stoi(port);
    //     return std::uint16_t(pn);
    // } catch(...) {
    //     return 0;
    // }
    return 0;
}

std::string
pptime(std::time_t const &t)
{
    if (!t) return "0";
    char str[15]; // 20161023 16:39
    if (std::strftime(str, sizeof(str), "%Y%m%d %H:%M", std::localtime(&t)))
        return std::string(str);
    return " - ";
    // return std::string(std::put_time(std::localtime(&t), "%Y%m%d %H:%M"));
}

std::uint32_t
randNum(std::uint32_t min, std::uint32_t max)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distrib(min, max);
    return distrib(rng);
}

} // namespace btd
