#pragma once

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#ifdef LOG_LEVEL_DEBUG
#define LOG_DEFAULT_LEVEL 0
#endif

#ifndef LOG_DEFAULT_LEVEL
#define LOG_DEFAULT_LEVEL 1
#endif

#ifdef LOGGING_ENABLED
#define CROW_ENABLE_LOGGING 1
#endif

namespace crow
{
enum class LogLevel
{
    Debug = 0,
    Info,    // 1
    Warning,
    Error,
    Critical,
};

class Logger
{
  private:
    //
    static std::string timestamp()
    {
        std::string date;
        date.resize(32, '\0');
        time_t t = time(nullptr);

        tm myTm{};

        gmtime_r(&t, &myTm);

        size_t sz =
            strftime(date.data(), date.size(), "%Y-%m-%d %H:%M:%S", &myTm);
        date.resize(sz);
        return date;
    }

  public:
    Logger([[maybe_unused]] const std::string& prefix,
           [[maybe_unused]] const std::string& filename,
           [[maybe_unused]] const size_t line, LogLevel levelIn) :
        level(levelIn)
    {
#ifdef CROW_ENABLE_LOGGING
        stringstream << "(" << timestamp() << ") [" << prefix << " "
                     << std::filesystem::path(filename).filename() << ":"
                     << line << "] ";
#endif
    }
    ~Logger()
    {
        if (level >= getCurrentLogLevel())
        {
#ifdef CROW_ENABLE_LOGGING
            stringstream << std::endl;
            std::cerr << stringstream.str();
#endif
        }
    }

    //
    template <typename T>
    Logger& operator<<([[maybe_unused]] T const& value)
    {
        if (level >= getCurrentLogLevel())
        {
#ifdef CROW_ENABLE_LOGGING
            stringstream << value;
#endif
        }
        return *this;
    }

    //
    static void setLogLevel(LogLevel level)
    {
        getLogLevelRef() = level;
    }

    static LogLevel getCurrentLogLevel()
    {
        return getLogLevelRef();
    }

  private:
    //
    static LogLevel& getLogLevelRef()
    {
        static auto currentLevel = static_cast<LogLevel>(LOG_DEFAULT_LEVEL);
        return currentLevel;
    }

    //
    std::ostringstream stringstream;
    LogLevel level;
};
} // namespace crow

#define CROW_LOG_CRITICAL                                                    \
    if (crow::Logger::getCurrentLogLevel() <= crow::LogLevel::Critical)        \
    crow::Logger("CRITICAL", __FILE__, __LINE__, crow::LogLevel::Critical)
#define CROW_LOG_ERROR                                                       \
    if (crow::Logger::getCurrentLogLevel() <= crow::LogLevel::Error)           \
    crow::Logger("ERROR", __FILE__, __LINE__, crow::LogLevel::Error)
#define CROW_LOG_WARNING                                                     \
    if (crow::Logger::getCurrentLogLevel() <= crow::LogLevel::Warning)         \
    crow::Logger("WARNING", __FILE__, __LINE__, crow::LogLevel::Warning)
#define CROW_LOG_INFO                                                        \
    if (crow::Logger::getCurrentLogLevel() <= crow::LogLevel::Info)            \
    crow::Logger("INFO", __FILE__, __LINE__, crow::LogLevel::Info)
#define CROW_LOG_DEBUG                                                       \
    if (crow::Logger::getCurrentLogLevel() <= crow::LogLevel::Debug)           \
    crow::Logger("DEBUG", __FILE__, __LINE__, crow::LogLevel::Debug)

#ifdef LOGGING_ENABLED
#define LOG_DEBUG CROW_LOG_DEBUG
#define LOG_ERROR CROW_LOG_ERROR
#define LOG_INFO CROW_LOG_INFO
#define LOG_WARNING CROW_LOG_WARNING
#endif
