#ifndef DFCUTILS_HPP
#define DFCUTILS_HPP

#include "netutils.hpp"
#include <array>
#include <string>
#include <vector>
#include <memory>

// DFC常量
constexpr const char* DFC_SERVER_CONF = "Server";
constexpr const char* DFC_USERNAME_CONF = "Username";
constexpr const char* DFC_PASSWORD_CONF = "Password";
constexpr const char* DFC_PASSWORD_DELIM = ": ";
constexpr const char* DFC_USERNAME_DELIM = ": ";

constexpr const char* DFC_LIST_CMD = "LIST";
constexpr const char* DFC_GET_CMD = "GET ";
constexpr const char* DFC_PUT_CMD = "PUT ";
constexpr const char* DFC_MKDIR_CMD = "MKDIR ";

// DFC常量枚举
enum DfcConstants {
    PASSWORD_FLAG = 0,
    USERNAME_FLAG = 1
};

// DFC服务器结构体
struct DfcServer {
    std::string name;
    std::string address;
    int port;
    
    DfcServer() : port(0) {}
};

// DFC配置结构体
struct DfcConfig {
    std::array<std::unique_ptr<DfcServer>, MAX_SERVERS> servers;
    std::unique_ptr<User> user;
    int server_count;
    
    DfcConfig() : server_count(0) {}
};

class DfcUtils {
public:
    // 连接管理
    static void setupConnections(std::vector<int>& connFds, const DfcConfig& conf);
    static void tearDownConnections(std::vector<int>& connFds, const DfcConfig& conf);
    static bool createConnections(std::vector<int>& connFds, const DfcConfig& conf);
    static int getDfcSocket(const DfcServer& server);
    
    // 命令构建和验证
    static bool commandBuilder(std::string& buffer, const std::string& format, 
                              const FileAttribute& fileAttr, const User& user, int flag);
    static void commandHandler(std::vector<int>& connFds, int flag, 
                              const std::string& buffer, DfcConfig& conf);
    static bool commandValidator(const std::string& buffer, int flag, FileAttribute& fileAttr);
    
    // 命令执行
    static void commandExec(std::vector<int>& connFds, const std::string& bufferToSend, 
                           int connCount, FileAttribute& attr, int flag, DfcConfig& conf);
    static bool sendCommand(const std::vector<int>& connFds, const std::string& bufferToSend, 
                           int connCount);
    
    // 文件操作
    static void sendFileSplits(int socket, const FileSplit& fileSplit, int mod, int serverIdx);
    static int fetchRemoteFileInfo(const std::vector<int>& connFds, int connCount, 
                                  ServerChunksCollate& serverChunksCollate);
    static void fetchRemoteSplits(std::vector<int>& connFds, int connCount, 
                                 FileSplit& fileSplit, int mod);
    static void fetchRemoteDirInfo(const std::vector<int>& connFds, int connCount);
    
    // 输出处理
    static void getOutputListCommand(const ServerChunksCollate& serverChunksCollate);
    static bool checkComplete(const std::array<bool, NUM_SERVER>& flagArray);
    
    // 认证功能
    static bool authConnections(const std::vector<int>& connFds, const DfcConfig& conf);
    
    // 配置文件处理
    static void readDfcConf(const std::string& filePath, DfcConfig& conf);
    static bool checkServerStruct(std::unique_ptr<DfcServer>& server);
    static void insertServerConf(const std::string& line, DfcConfig& conf);
    static void insertUserConf(const std::string& line, DfcConfig& conf, 
                              const std::string& delim, int flag);
    
    // 文件分割处理
    static bool splitFileToPieces(const std::string& filePath, FileSplit& fileSplit);
    static bool combineFileFromPieces(const FileAttribute& fileAttr, const FileSplit& fileSplit);
    
    // 调试和内存管理
    static void printDfcConf(const DfcConfig& conf);
    static void freeDfcConf(DfcConfig& conf);
    static void freeDfcServer(DfcServer& server);

private:
    // 文件映射表
    static const int filePiecesMapping[4][4][2];
};

#endif // DFCUTILS_HPP