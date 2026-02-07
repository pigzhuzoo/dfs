#include "../include/dfcutils.hpp"
#include <iostream>
#include <cstring>

int main(int argc, char** argv) {
    DfcConfig conf;
    std::string confFile, buffer;
    std::vector<int> connFds;
    FileSplit fileSplit;
    
    if (argc != 2) {
        std::cerr << "USAGE: dfc <conf_file>" << std::endl;
        exit(1);
    }
    
    confFile = argv[1];
    DfcUtils::readDfcConf(confFile, conf);
    
    while (true) {
        buffer.clear();
        memset(&fileSplit, 0, sizeof(fileSplit));
        
        std::cout << ">>> " << std::flush;  // 添加flush确保提示符立即显示
        std::getline(std::cin, buffer);
        
        // 检查退出命令
        if (buffer == "EXIT" || buffer == "QUIT" || buffer == "exit" || buffer == "quit") {
            std::cout << "<<< Goodbye!" << std::endl;
            break;
        }
        
        // 初始化连接向量
        DfcUtils::setupConnections(connFds, conf);
        
        if (buffer.substr(0, 4) == "LIST") {
            std::string cmdArgs = buffer.substr(4);
            DEBUGSS("Command Sent is LIST", cmdArgs.c_str());
            DfcUtils::commandHandler(connFds, LIST_FLAG, cmdArgs, conf);
            
        } else if (buffer.substr(0, 4) == "GET ") {
            std::string cmdArgs = buffer.substr(4);
            DEBUGSS("Command Sent is GET", cmdArgs.c_str());
            DfcUtils::commandHandler(connFds, GET_FLAG, cmdArgs, conf);
            
        } else if (buffer.substr(0, 4) == "PUT ") {
            std::string cmdArgs = buffer.substr(4);
            DEBUGSS("Command Sent is PUT", cmdArgs.c_str());
            DfcUtils::commandHandler(connFds, PUT_FLAG, cmdArgs, conf);
            
        } else if (buffer.substr(0, 6) == "MKDIR ") {
            std::string cmdArgs = buffer.substr(6);
            DEBUGSS("Command Sent is MKDIR", cmdArgs.c_str());
            DfcUtils::commandHandler(connFds, MKDIR_FLAG, cmdArgs, conf);
        } else if (!buffer.empty()) {
            DEBUGSS("Invalid Command", buffer.c_str());
            std::cout << "<<< Invalid command. Available commands: LIST, GET, PUT, MKDIR, EXIT/QUIT" << std::endl;
        }
        
        // 清理连接
        DfcUtils::tearDownConnections(connFds, conf);
    }
    
    DfcUtils::freeDfcConf(conf);
    return 0;
}