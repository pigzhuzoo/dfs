#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <iostream>
#include <string>
#include "../include/logger.hpp"

// 使用新的日志函数替代直接的std::cerr输出
#define DEBUGS(s) log_debug(std::string(" ") + s)
#define DEBUGN(s) log_debug(std::to_string(s))
#define DEBUGSN(d, s) log_debug(std::string(d) + ": " + std::to_string(s))
#define DEBUGSS(d, s) log_debug(std::string(d) + ": " + std::string(s))

#endif // DEBUG_HPP