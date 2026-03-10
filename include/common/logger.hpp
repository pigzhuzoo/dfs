#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <memory>

class Logger {
public:
    explicit Logger(const std::string& log_file_path);
    ~Logger();
    
    void log(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);
    
    static void set_debug_enabled(bool enabled);
    static bool is_debug_enabled();

private:
    std::ofstream log_file_;
    std::mutex log_mutex_;
    std::string get_timestamp();
    static bool debug_enabled_;
};

// 初始化Logger实例
void init_logger(int port);

// 全局日志函数
void log_info(const std::string& message);
void log_error(const std::string& message);
void log_debug(const std::string& message);

#endif // LOGGER_HPP