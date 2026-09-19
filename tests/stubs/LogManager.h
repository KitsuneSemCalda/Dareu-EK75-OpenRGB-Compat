#pragma once
#include <string>
#include <vector>

enum { LL_FATAL, LL_ERROR, LL_WARNING, LL_INFO, LL_VERBOSE, LL_DEBUG, LL_TRACE, LL_DIALOG };

void LogAppend(int level, const char* fmt, ...);

/*---------------------------------------------------------*\
| Everything logged, "<level> <message>", for assertions    |
\*---------------------------------------------------------*/
extern std::vector<std::string> g_test_log;

#define LOG_FATAL(...)      LogAppend(LL_FATAL,   __VA_ARGS__)
#define LOG_ERROR(...)      LogAppend(LL_ERROR,   __VA_ARGS__)
#define LOG_WARNING(...)    LogAppend(LL_WARNING, __VA_ARGS__)
#define LOG_INFO(...)       LogAppend(LL_INFO,    __VA_ARGS__)
#define LOG_VERBOSE(...)    LogAppend(LL_VERBOSE, __VA_ARGS__)
#define LOG_DEBUG(...)      LogAppend(LL_DEBUG,   __VA_ARGS__)
#define LOG_TRACE(...)      LogAppend(LL_TRACE,   __VA_ARGS__)
#define LOG_DIALOG(...)     LogAppend(LL_DIALOG,  __VA_ARGS__)
