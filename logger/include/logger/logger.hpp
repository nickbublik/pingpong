#pragma once

#include <iostream>

namespace Net
{
namespace Logger
{

inline void outputDebug(const char *file, unsigned line)
{
    std::cout << "[DBG] " << file << ":" << line << ": ";
}

template <typename... Args>
inline void outputDebug(const char *file, unsigned line, Args &&...args)
{
    outputDebug(file, line);
    (std::cout << ... << std::forward<Args>(args)) << '\n';
}

inline void doNothing() {}

#if ENABLE_DEBUG_LOG
#define DBG_LOG(...) Net::Logger::outputDebug(__FILE__, __LINE__, __VA_ARGS__)
#else
#define DBG_LOG(...) (Net::Logger::doNothing())
#endif

} // namespace Logger
} // namespace Net
