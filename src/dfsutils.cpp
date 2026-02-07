#include "../include/dfsutils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <fstream>
#include <sstream>

int DfsUtils::getDfsSocket(int portNumber) {
    int sockfd;
    struct sockaddr_in sin;
    int yes = 1;
    
    memset(&sin, 0, sizeof(sin));
    
    sin.sin_family = AF_INET;
    sin.sin_port = htons(portNumber);
    sin.sin_addr.s_addr = INADDR_ANY;
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to start socket:");
        exit(1);
    }
    
    // 避免"地址已在使用中"错误消息
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("Unable to set so_reuseaddr:");
        exit(1);
    }
    
    if (bind(sockfd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("Unable to bind the socket:");
        exit(1);
    }
    
    if (listen(sockfd, MAX_CONNECTION) < 0) {
        perror("Unable to call listen on socket:");
        exit(1);
    }
    
    return sockfd;
}

bool DfsUtils::authDfsUser(const User& user, const DfsConfig& conf) {
    for (int i = 0; i < conf.user_count; i++) {
        if (conf.users[i] && *conf.users[i] == user) {
            return true;
        }
    }
    return false;
}

void DfsUtils::dfsCommandAccept(int socket, DfsConfig& conf) {
    log_debug("dfsCommandAccept called");  // 在最开始添加日志
    
    std::vector<unsigned char> buffer(MAX_SEG_SIZE, 0);
    std::string tempBuffer(MAX_SEG_SIZE, 0);
    DfsRecvCommand dfsRecvCommand;
    
    // 接收命令
    int commandSize;
    NetUtils::recvIntValueSocket(socket, commandSize);
    
    std::stringstream ss1;
    ss1 << "Received command size: " << commandSize;
    log_debug(ss1.str());
    
    buffer.resize(commandSize);
    NetUtils::recvFromSocket(socket, buffer);
    
    log_debug("Received command buffer");
    
    // 将缓冲区转换为字符串
    std::string commandStr(buffer.begin(), buffer.end());
    
    std::stringstream ss2;
    ss2 << "Command string: " << commandStr;
    log_debug(ss2.str());
    
    // 解析临时模板获取标志
    int tempFlag;
    sscanf(commandStr.c_str(), GENERIC_TEMPLATE, &tempFlag, tempBuffer.data());
    dfsRecvCommand.flag = tempFlag;
    
    int flag = dfsRecvCommand.flag;
    bool authFlag = false;
    
    log_debug("Decoding and authentication command");
    
    if (flag == LIST_FLAG) {
        log_info("Command Received is LIST");
        authFlag = dfsCommandDecodeAndAuth(commandStr, LIST_TEMPLATE, dfsRecvCommand, conf);
    } else if (flag == GET_FLAG) {
        log_info("Command Received is GET");
        authFlag = dfsCommandDecodeAndAuth(commandStr, GET_TEMPLATE, dfsRecvCommand, conf);
    } else if (flag == PUT_FLAG) {
        log_info("Command Received is PUT");
        authFlag = dfsCommandDecodeAndAuth(commandStr, PUT_TEMPLATE, dfsRecvCommand, conf);
    } else if (flag == MKDIR_FLAG) {
        log_info("Command Received is MKDIR");
        authFlag = dfsCommandDecodeAndAuth(commandStr, MKDIR_TEMPLATE, dfsRecvCommand, conf);
    }
    
    if (!authFlag) {
        NetUtils::sendIntValueSocket(socket, -1);
        sendError(socket, AUTH_FAILED);
    } else {
        NetUtils::sendIntValueSocket(socket, 0);  // 发送成功确认
        dfsCommandExec(socket, dfsRecvCommand, conf, flag);
    }
}

bool DfsUtils::dfsCommandDecodeAndAuth(const std::string& buffer, const std::string& format, 
                                      DfsRecvCommand& recvCmd, const DfsConfig& conf) {
    char username[MAX_CHAR_BUFF], password[MAX_CHAR_BUFF], folder[MAX_CHAR_BUFF], fileName[MAX_CHAR_BUFF];
    
    sscanf(buffer.c_str(), format.c_str(), &recvCmd.flag, username, password, folder, fileName);
    
    recvCmd.user.username = std::string(username);
    recvCmd.user.password = std::string(password);
    recvCmd.folder = std::string(folder);
    recvCmd.file_name = std::string(fileName);
    
    // 处理在客户端收到"NULL"的情况
    if (Utils::compareString(recvCmd.folder, "NULL")) {
        recvCmd.folder.clear();
    }
    
    if (Utils::compareString(recvCmd.file_name, "NULL")) {
        recvCmd.file_name.clear();
    }
    
    return authDfsUser(recvCmd.user, conf);
}

bool DfsUtils::dfsCommandExec(int socket, const DfsRecvCommand& recvCmd, 
                             DfsConfig& conf, int flag) {
    std::string folderPath;
    unsigned char payloadBuffer[MAX_SEG_SIZE];
    ServerChunksInfo serverChunksInfo;
    std::array<Split, 2> splits;
    int sizeOfPayload, splitId;
    unsigned char signal;
    bool folderPathFlag, fileFlag;
    
    // 创建用户名目录（如果不存在）
    folderPath = conf.server_name + "/" + recvCmd.user.username;
    
    if (!Utils::checkDirectoryExists(folderPath)) {
        DEBUGSS("Creating user directory:", folderPath.c_str());
        log_debug("Creating user directory: " + folderPath);
        createDfsDirectory(folderPath);
    }
    
    // 构建完整路径
    // 如果recvCmd.folder是"/"或空字符串，则使用用户目录作为根目录
    if (recvCmd.folder.empty() || recvCmd.folder == "/") {
        folderPath = conf.server_name + "/" + recvCmd.user.username;
    } else {
        folderPath = conf.server_name + "/" + recvCmd.user.username + "/" + recvCmd.folder;
    }
    
    // 处理路径末尾的斜杠（确保不以斜杠结尾，除非是根目录）
    if (folderPath.length() > 1 && folderPath.back() == '/') {
        folderPath.pop_back();
    }
    
    DEBUGSS("Folder Path from Request", folderPath.c_str());
    log_debug("Folder Path from Request: " + folderPath);
    
    // 检查文件夹路径是否存在
    folderPathFlag = Utils::checkDirectoryExists(folderPath);
    
    if (flag == LIST_FLAG) {
        if (!folderPathFlag) {
            log_debug("Folder path doesn't exist and sending back error message");
            NetUtils::sendIntValueSocket(socket, -1);
            sendError(socket, FOLDER_NOT_FOUND);
            return false;
        }
        
        log_debug("Reading all the files in the folder path from request");
        Utils::getFilesInFolder(folderPath, serverChunksInfo, "");
        
        // 发送hasData标志：1表示有文件数据，0表示无文件数据
        int hasData = (serverChunksInfo.chunks > 0) ? 1 : 0;
        
        // 调试：显示hasData的实际字节
        std::vector<unsigned char> hasDataBuffer(INT_SIZE);
        NetUtils::encodeIntToUchar(hasDataBuffer, hasData);
        std::stringstream ss_hasData;
        ss_hasData << "hasData bytes: ";
        for (int i = 0; i < INT_SIZE; i++) {
            ss_hasData << (int)hasDataBuffer[i] << " ";
        }
        log_debug(ss_hasData.str());

        NetUtils::sendIntValueSocket(socket, hasData);
        
        // 总是发送payloadSize，即使没有文件数据
        int sizeOfPayload = INT_SIZE + serverChunksInfo.chunks * CHUNK_INFO_STRUCT_SIZE;
        
        // 调试：显示sizeOfPayload的实际字节
        std::vector<unsigned char> sizeBuffer(INT_SIZE);
        NetUtils::encodeIntToUchar(sizeBuffer, sizeOfPayload);
        std::stringstream ss_size;
        ss_size << "sizeOfPayload bytes: ";
        for (int i = 0; i < INT_SIZE; i++) {
            ss_size << (int)sizeBuffer[i] << " ";
        }
        log_debug(ss_size.str());

        NetUtils::sendIntValueSocket(socket, sizeOfPayload);
        
        if (hasData) {
            log_debug("Sending files info to the client");
            std::vector<unsigned char> uCharBuffer(sizeOfPayload);
            NetUtils::encodeServerChunksInfoToBuffer(uCharBuffer, serverChunksInfo);
            
            // 添加缓冲区内容调试
            std::stringstream ss4;
            ss4 << "Sending buffer of size " << uCharBuffer.size() << ": ";
            for (size_t i = 0; i < uCharBuffer.size(); i++) {
                ss4 << (int)uCharBuffer[i] << " ";
            }
            log_debug(ss4.str());
            
            NetUtils::sendToSocket(socket, uCharBuffer);
        } else {
            // 发送空的payload
            std::vector<unsigned char> emptyPayload(sizeOfPayload, 0);
            
            // 添加缓冲区内容调试
            std::stringstream ss5;
            ss5 << "Sending empty buffer of size " << emptyPayload.size() << ": ";
            for (size_t i = 0; i < emptyPayload.size(); i++) {
                ss5 << (int)emptyPayload[i] << " ";
            }
            log_debug(ss5.str());
            
            NetUtils::sendToSocket(socket, emptyPayload);
        }
        
        // 发送文件夹信息
        std::vector<unsigned char> folderPayload;
        sizeOfPayload = Utils::getFoldersInFolder(folderPath, folderPayload);
        NetUtils::sendIntValueSocket(socket, sizeOfPayload);
        if (sizeOfPayload > 0) {
            NetUtils::sendToSocket(socket, folderPayload);
        }
        
        // 修复：等待客户端信号后再关闭连接，避免客户端接收时连接被关闭
        unsigned char signal;
        log_debug("Waiting for signal from client after LIST command");
        NetUtils::recvSignal(socket, signal);
        // 无论收到什么信号，都正常结束
        
    } else if (flag == GET_FLAG) {
        if (!folderPathFlag) {
            log_debug("Folder path doesn't exist and sending back error message");
            NetUtils::sendIntValueSocket(socket, -1);
            sendError(socket, FOLDER_NOT_FOUND);
            return false;
        }
        
        log_debug("Reading given file from folder path from request");
        fileFlag = Utils::getFilesInFolder(folderPath, serverChunksInfo, recvCmd.file_name);
        
        // Modified: Always send response even if file doesn't exist locally
        // This allows client to collect info from all servers and determine correct MOD
        sizeOfPayload = INT_SIZE + serverChunksInfo.chunks * CHUNK_INFO_STRUCT_SIZE;

        NetUtils::sendIntValueSocket(socket, 1);
        log_debug("Sending the file's info to the client");

        // 修复：先发送payloadSize
        NetUtils::sendIntValueSocket(socket, sizeOfPayload);
        
        std::vector<unsigned char> uCharBuffer(sizeOfPayload);
        NetUtils::encodeServerChunksInfoToBuffer(uCharBuffer, serverChunksInfo);
        NetUtils::sendToSocket(socket, uCharBuffer);
        
        log_debug("Waiting for signal from client");
        NetUtils::recvSignal(socket, signal);
        
        if (signal == PROCEED_SIG) {
            log_info("Proceeding with sending file split as requested by client");
            while (true) {
                NetUtils::recvIntValueSocket(socket, splitId);
                
                // 修复：正确的分片文件路径应该包含目录分隔符和隐藏文件前缀
                std::string splitPath = folderPath + "/." + recvCmd.file_name + "." + std::to_string(splitId);
                splits[0].id = splitId;
                Utils::readIntoSplitFromFile(splitPath, splits[0]);
                NetUtils::writeSplitToSocketAsStream(socket, splits[0]);
                Utils::freeSplit(splits[0]);
                
                // 接收RESET_SIG，然后继续循环处理下一个请求
                NetUtils::recvSignal(socket, signal);
                if (signal != RESET_SIG) {
                    // 如果不是RESET_SIG，可能是其他信号，退出循环
                    break;
                }
            }
        } else {
            log_debug("Client sent unexpected signal, not proceeding");
        }
        
    } else if (flag == PUT_FLAG) {
        log_info("Handling PUT command for user: " + recvCmd.user.username + 
                ", file: " + recvCmd.file_name + ", folder: " + recvCmd.folder);
        std::cout << "DEBUG: Handling PUT command" << std::endl;
        
        // 确保目标目录存在
        if (!Utils::checkDirectoryExists(folderPath)) {
            log_debug("Creating directory for PUT: " + folderPath);
            createDfsDirectory(folderPath);
        }
        
        // 服务器不需要验证文件是否存在，直接接收分片
        // 接收2个分片（根据当前设计）
        log_debug("Starting to receive 2 splits for PUT operation");
        for (int i = 0; i < 2; i++) {
            log_debug("Receiving split " + std::to_string(i + 1) + "/2");
            memset(payloadBuffer, 0, MAX_SEG_SIZE);
            try {
                NetUtils::writeSplitFromSocketAsStream(socket, splits[i]);
                log_debug("Successfully received split " + std::to_string(i + 1) + 
                         ", ID: " + std::to_string(splits[i].id) + 
                         ", content_length: " + std::to_string(splits[i].content_length));
                Utils::writeSplitToFile(splits[i], folderPath, recvCmd.file_name);
                log_debug("Successfully wrote split " + std::to_string(i + 1) + " to file");
                Utils::freeSplit(splits[i]);
            } catch (const std::exception& e) {
                log_error("Error receiving split " + std::to_string(i + 1) + ": " + e.what());
                // 发送错误响应
                NetUtils::sendIntValueSocket(socket, -1);
                sendError(socket, FILE_NOT_FOUND);
                return false;
            }
        }
        
        // 发送成功响应
        log_info("PUT operation completed successfully for file: " + recvCmd.file_name);
        NetUtils::sendIntValueSocket(socket, 1);
        std::cout << "DEBUG: PUT operation completed" << std::endl;
    } else if (flag == MKDIR_FLAG) {
        if (folderPathFlag) {
            log_debug("Folder path already exists");
            NetUtils::sendIntValueSocket(socket, -1);
            sendError(socket, FOLDER_EXISTS);
            return false;
        }
        NetUtils::sendIntValueSocket(socket, 1);
        log_info("Creating directory");
        createDfsDirectory(folderPath);
    }
    
    return true;
}

void DfsUtils::sendErrorHelper(int socket, const std::string& message) {
    int payloadSize = message.length();
    std::vector<unsigned char> payload(payloadSize);
    
    std::copy(message.begin(), message.end(), payload.begin());
    NetUtils::sendIntValueSocket(socket, payloadSize);
    NetUtils::sendToSocket(socket, payload);
}

void DfsUtils::sendError(int socket, int flag) {
    switch (flag) {
        case FOLDER_NOT_FOUND:
            sendErrorHelper(socket, FOLDER_NOT_FOUND_ERROR);
            break;
        case FOLDER_EXISTS:
            sendErrorHelper(socket, FOLDER_EXISTS_ERROR);
            break;
        case FILE_NOT_FOUND:
            sendErrorHelper(socket, FILE_NOT_FOUND_ERROR);
            break;
        case AUTH_FAILED:
            sendErrorHelper(socket, AUTH_FAILED_ERROR);
            break;
        default:
            DEBUGS("Unknown Error Flag");
            break;
    }
}

void DfsUtils::readDfsConf(const std::string& filePath, DfsConfig& conf) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        perror("DFC => Error in opening config file: ");
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        insertDfsUserConf(line, conf);
    }
    
    file.close();
}

void DfsUtils::insertDfsUserConf(const std::string& line, DfsConfig& conf) {
    size_t spacePos = line.find(' ');
    if (spacePos != std::string::npos) {
        std::string username = line.substr(0, spacePos);
        std::string password = line.substr(spacePos + 1);
        
        int i = conf.user_count++;
        conf.users[i] = std::make_unique<User>();
        conf.users[i]->username = username;
        conf.users[i]->password = password;
    }
}

int DfsUtils::mkdirRecursive(const std::string& path, mode_t mode) {
    std::string tempPath = path;
    
    for (size_t i = 1; i < tempPath.length(); i++) {
        if (tempPath[i] == '/') {
            tempPath[i] = '\0';
            if (tempPath != "/" && mkdir(tempPath.c_str(), mode) == -1) {
                if (errno != EEXIST) {
                    return -1;
                }
            }
            tempPath[i] = '/';
        }
    }
    
    if (mkdir(tempPath.c_str(), mode) == -1) {
        return (errno == EEXIST) ? 0 : -1;
    }
    return 0;
}

void DfsUtils::createDfsDirectory(const std::string& path) {
    struct stat st;
    int res = -1;
    errno = 0;
    
    if (stat(path.c_str(), &st) == -1) {
        res = mkdirRecursive(path, 0755);
        if (res == 0) {
            errno = 0;
        }
    } else {
        res = 0;
        errno = 0;
    }
    
    printf("Created! %s %d | errno=%d, err_desc=%s\n", 
           path.c_str(), res, errno, strerror(errno));
}

void DfsUtils::dfsDirectoryCreator(const std::string& serverName, DfsConfig& conf) {
    createDfsDirectory(serverName);
    for (int i = 0; i < conf.user_count; i++) {
        if (conf.users[i]) {
            std::string filePath = serverName + "/" + conf.users[i]->username;
            createDfsDirectory(filePath);
        }
    }
}

void DfsUtils::printDfsConf(const DfsConfig& conf) {
    for (int i = 0; i < conf.user_count; i++) {
        if (conf.users[i]) {
            std::cerr << "Username: " << conf.users[i]->username 
                      << " & Password: " << conf.users[i]->password << std::endl;
        }
    }
}

void DfsUtils::freeDfsConf(DfsConfig& conf) {
    for (int i = 0; i < conf.user_count; i++) {
        conf.users[i].reset();
    }
    conf.user_count = 0;
}