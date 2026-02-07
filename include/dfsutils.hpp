#ifndef DFSUTILS_HPP
#define DFSUTILS_HPP

#include "netutils.hpp"
#include "logger.hpp"
#include <array>
#include <string>
#include <vector>

// DFS常量
constexpr int MAX_USERS = 10;
constexpr int MAX_CONNECTION = 10;

// 错误代码枚举
enum DfsError {
    FOLDER_NOT_FOUND = 1,
    FOLDER_EXISTS = 2,
    FILE_NOT_FOUND = 3,
    AUTH_FAILED = 4
};

// 错误消息常量
constexpr const char* FOLDER_NOT_FOUND_ERROR = "Requested folder does not exists on server";
constexpr const char* FOLDER_EXISTS_ERROR = "Requested folder already exists on server";
constexpr const char* FILE_NOT_FOUND_ERROR = "Requested file does not exists on server";
constexpr const char* AUTH_FAILED_ERROR = "Invalid Username/Password. Please try again";

// DFS配置结构体
struct DfsConfig {
    std::string server_name;
    std::array<std::unique_ptr<User>, MAX_USERS> users;
    int user_count;
    
    DfsConfig() : user_count(0) {}
};

// DFS接收命令结构体
struct DfsRecvCommand {
    int flag;
    User user;
    std::string folder;     // 文件夹总是以"/"结尾，不以"/"开头
    std::string file_name;
    
    DfsRecvCommand() : flag(0) {}
};

class DfsUtils {
public:
    // Socket操作
    static int getDfsSocket(int portNumber);
    
    // 认证功能
    static bool authDfsUser(const User& user, const DfsConfig& conf);
    
    // 命令处理
    static void dfsCommandAccept(int socket, DfsConfig& conf);
    static bool dfsCommandDecodeAndAuth(const std::string& buffer, const std::string& format, 
                                       DfsRecvCommand& recvCmd, const DfsConfig& conf);
    static bool dfsCommandExec(int socket, const DfsRecvCommand& recvCmd, 
                              DfsConfig& conf, int flag);
    
    // 目录管理
    static void createDfsDirectory(const std::string& path);
    static void dfsDirectoryCreator(const std::string& serverName, DfsConfig& conf);
    
    // 错误处理
    static void sendErrorHelper(int socket, const std::string& message);
    static void sendError(int socket, int flag);
    
    // 配置文件处理
    static void readDfsConf(const std::string& filePath, DfsConfig& conf);
    static void insertDfsUserConf(const std::string& line, DfsConfig& conf);
    
    // 调试功能
    static void printDfsConf(const DfsConfig& conf);
    static void freeDfsConf(DfsConfig& conf);

private:
    // 内部辅助函数
    static int mkdirRecursive(const std::string& path, mode_t mode);
};

#endif // DFSUTILS_HPP