
#include <cinttypes> // for PRId64 et.al.
#include <cstdarg> // va_start, va_end
#include <cstdio> // FILE
#include <chrono> // time and clock
#include <dirent.h> // readdir
#include <mutex>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>
#include <string.h> // strdup

#ifndef _WIN32
#include <pwd.h>
#endif

#include <libtorrent/config.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/string_view.hpp>
#include <libtorrent/time.hpp>

#include "util.hpp"

namespace btd {
using namespace std::chrono;
static std::time_t start_up = std::time(0);
static time_point start_up_c = high_resolution_clock::now();

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
    auto cur_c = high_resolution_clock::now();
    auto int_ms = duration_cast<milliseconds>(cur_c - start_up_c);
    return int_ms.count();
}

static struct logger
{
    FILE* log_file;
    std::mutex log_mutex;

    logger() : log_file(nullptr) {}
    ~logger()
    {
        if (log_file) fclose(log_file);
    }
} log_file_holder;

void log(char const* fmt, ...)
{
    if (log_file_holder.log_file == nullptr) return;

    std::lock_guard<std::mutex> lock(log_file_holder.log_mutex);
    static lt::time_point start = lt::clock_type::now();
    std::fprintf(log_file_holder.log_file, "[%010" PRId64 "] ", lt::total_microseconds(lt::clock_type::now() - start));
    va_list l;
    va_start(l, fmt);
    vfprintf(log_file_holder.log_file, fmt, l);
    va_end(l);
}

bool is_logging() {
    return log_file_holder.log_file != nullptr;
}

void set_logging(bool enable) {
    if (enable)
    {
        if (log_file_holder.log_file == nullptr)
        {
            auto fn = getLogsDir()+"/kedge.log";
            log_file_holder.log_file = fopen(fn.c_str(), "w+");
        }
    }
    else
    {
        if (log_file_holder.log_file != nullptr)
        {
            FILE* f = log_file_holder.log_file;
            log_file_holder.log_file = nullptr;
            fclose(f);
        }
    }
}


char const* timestamp()
{
    time_t t = std::time(nullptr);
    tm* timeinfo = std::localtime(&t);
    static char str[200];
    std::strftime(str, 200, "%m%d %X", timeinfo);
    return str;
}


bool is_resume_file(std::string const& s)
{
    static std::string const hex_digit = "0123456789abcdef";
    if (s.size() != 40 + 7) return false;
    if (s.substr(40) != ".resume") return false;
    for (char const c : s.substr(0, 40))
    {
        if (hex_digit.find(c) == std::string::npos) return false;
    }
    return true;
}


bool load_file(std::string const& filename, std::vector<char>& v, int limit)
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

int save_file(std::string const& filename, std::vector<char> const& v)
{
    std::fstream f(filename, std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
    f.write(v.data(), v.size());
    return !f.fail();
}


std::string const
path_cat(std::string_view const& base, std::string_view const& path)
{
    auto result = fs::path(base)+=path;
    return result.string();
}

bool
from_hex(lt::sha1_hash & ih, std::string const &s)
{
    if (s.size() != 40)
    {
        return false;
    }
    std::stringstream hash(s.c_str());
    hash >> ih;
    if (hash.fail()) {
        return false;
    }
    return true;
}

std::string
to_hex(lt::sha1_hash const& ih)
{
    std::stringstream ret;
    ret << ih;
    return ret.str();
}

std::string const
getEnvStr( std::string const & key, std::string const & dft )
{
    char * val = std::getenv( key.c_str() );
    return val == nullptr ? dft : std::string(val);
}

static std::string
getHomeDir()
{
    static char * home = nullptr;
    if (home == nullptr)
    {
        home = std::getenv("HOME");
        if (home == nullptr)
        {
            struct passwd* pw = getpwuid(getuid());
            if (pw != NULL)
            {
                home = strdup(pw->pw_dir);
            }
            endpwent();
        }
        if (home == nullptr)
        {
            home = strdup("");
        }
    }

    return home;
}


std::string const
getConfDir(std::string const& app)
{
    // if (app.size() == 0)
    // {
    //  app = DEFAULT_APP_NAME;
    // }
    // TODO: _WIN32
#ifdef __APPLE__
    auto dir = fs::path(getHomeDir()) / "Library" / "Application Support" / app;
#else
    auto xcfg = fs::path(getHomeDir()) / ".config";
    auto dir = fs::path(getEnvStr("XDG_CONFIG_HOME", xcfg.string())) / app;
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
    return getEnvStr(ENV_KEDGE_WEB_UI, dir.string());
}

std::int16_t
parse_port(std::string const& addr) noexcept
{
    if (addr.empty())
    {
        return 0;
    }
    char* port = (char*) strrchr((char*)addr.c_str(), ':');
    if (port != nullptr)
    {
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
pptime(std::time_t const & t)
{
    if (!t) return "0";
    char str[15]; // 20161023 16:39
    if (std::strftime(str, sizeof(str), "%Y%m%d %H:%M", std::localtime(&t)))
        return std::string(str);
    return " - ";
    // return std::string(std::put_time(std::localtime(&t), "%Y%m%d %H:%M"));
}

} // namespace
