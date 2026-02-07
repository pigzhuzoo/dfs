#include "../include/dfsutils.hpp"
#include "../include/logger.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    pid_t pid;
    DfsConfig conf;
    std::string serverFolder, fileName = "conf/dfs.conf";
    int portNumber, listenFd, connFd, status;
    struct sockaddr_in remoteAddress;
    socklen_t addrSize = sizeof(struct sockaddr_in);
    
    if (argc != 3) {
        std::cerr << "USAGE: dfs <folder> <port>" << std::endl;
        exit(1);
    }
    
    serverFolder = argv[1];
    portNumber = atoi(argv[2]);
    
    // 初始化对应端口的日志文件
    init_logger(portNumber);
    
    DfsUtils::readDfsConf(fileName, conf);
    // 如果serverFolder以'/'开头，则去掉它，否则直接使用
    if (!serverFolder.empty() && serverFolder[0] == '/') {
        conf.server_name = serverFolder.substr(1);
    } else {
        conf.server_name = serverFolder;
    }
    
    // 创建DFS目录（如果需要的话）
    DfsUtils::dfsDirectoryCreator(conf.server_name, conf);
    
    listenFd = DfsUtils::getDfsSocket(portNumber);
    
    while (true) {
        DEBUGSS("Waiting to Accept Connection", serverFolder.c_str());
        if ((connFd = accept(listenFd, (struct sockaddr*)&remoteAddress, &addrSize)) <= 0) {
            perror("Error Accepting Connection");
            continue;
        }
        
        pid = fork();
        if (pid != 0) {
            close(connFd);
            waitpid(-1, &status, WNOHANG);
        } else {
            // 子进程中也需要初始化日志
            init_logger(portNumber);
            DEBUGSN("In Child process", getpid());
            DfsUtils::dfsCommandAccept(connFd, conf);
            close(connFd);
            break;
        }
        DEBUGS("Closed Connection, waiting to accept next");
    }
    
    DfsUtils::freeDfsConf(conf);
    return 0;
}