#include "../include/logger.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <unordered_map>

// 使用线程局部存储来支持每个服务器实例有自己的Logger
thread_local static std::unique_ptr<Logger> g_logger;

// 全局映射，用于在主线程中管理Logger实例
static std::unordered_map<int, std::unique_ptr<Logger>> g_loggers;
static std::mutex g_loggers_mutex;

Logger::Logger(const std::string& log_file_path) {
    log_file_.open(log_file_path, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path << std::endl;
    }
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

std::string Logger::get_timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* tm_now = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm_now, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::string timestamp = get_timestamp();
    std::string log_message = "[" + timestamp + "] [INFO] " + message + "\n";
    if (log_file_.is_open()) {
        log_file_ << log_message;
        log_file_.flush();
    }
    std::cout << log_message;
}

void Logger::error(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::string timestamp = get_timestamp();
    std::string log_message = "[" + timestamp + "] [ERROR] " + message + "\n";
    if (log_file_.is_open()) {
        log_file_ << log_message;
        log_file_.flush();
    }
    std::cerr << log_message;
}

void Logger::debug(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::string timestamp = get_timestamp();
    std::string log_message = "[" + timestamp + "] [DEBUG] " + message + "\n";
    if (log_file_.is_open()) {
        log_file_ << log_message;
        log_file_.flush();
    }
    std::cerr << log_message;
}

// 初始化Logger实例
void init_logger(int port) {
    std::string log_file_path = "logs/dfs" + std::to_string(port - 10000) + ".log";
    g_logger = std::make_unique<Logger>(log_file_path);
    
    // 在主线程中也保存一份
    std::lock_guard<std::mutex> lock(g_loggers_mutex);
    g_loggers[port] = std::make_unique<Logger>(log_file_path);
}

// 全局日志函数实现
void log_info(const std::string& message) {
    if (g_logger) {
        g_logger->log(message);
    } else {
        // 如果没有初始化，输出到标准输出
        std::cout << "[INFO] " << message << std::endl;
    }
}

void log_error(const std::string& message) {
    if (g_logger) {
        g_logger->error(message);
    } else {
        std::cerr << "[ERROR] " << message << std::endl;
    }
}

void log_debug(const std::string& message) {
    if (g_logger) {
        g_logger->debug(message);
    } else {
        std::cerr << "[DEBUG] " << message << std::endl;
    }
}