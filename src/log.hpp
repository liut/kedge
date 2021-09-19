#pragma once

#ifdef LOG_LEVEL_DEBUG
#ifndef LOG_DEFAULT_LEVEL
#define LOG_DEFAULT_LEVEL 0
#endif
#endif

#ifdef LOGGING_ENABLED
#define CROW_ENABLE_LOGGING 1
#endif

#include "crow/logging.hpp"

#define LOG_DEBUG CROW_LOG_DEBUG
#define LOG_ERROR CROW_LOG_ERROR
#define LOG_INFO CROW_LOG_INFO
#define LOG_WARNING CROW_LOG_WARNING

