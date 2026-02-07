#include "../include/dfcutils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/time.h>
#include <fstream>
#include <sys/stat.h>
#include <set>
#include <sstream>

// 文件映射表定义
const int DfcUtils::filePiecesMapping[4][4][2] = {
    { { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 1 } },
    { { 4, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 } },
    { { 3, 4 }, { 4, 1 }, { 1, 2 }, { 2, 3 } },
    { { 2, 3 }, { 3, 4 }, { 4, 1 }, { 1, 2 } }
};

void DfcUtils::setupConnections(std::vector<int>& connFds, const DfcConfig& conf) {
    connFds.resize(conf.server_count, -1);  // 确保向量有足够的空间
    createConnections(connFds, conf);
}

void DfcUtils::tearDownConnections(std::vector<int>& connFds, const DfcConfig& conf) {
    for (int i = 0; i < conf.server_count; i++) {
        if (connFds[i] != -1) {
            close(connFds[i]);
            connFds[i] = -1;
        }
    }
}

bool DfcUtils::createConnections(std::vector<int>& connFds, const DfcConfig& conf) {
    bool connectionFlag = false;
    for (int i = 0; i < conf.server_count; i++) {
        if (conf.servers[i]) {
            connFds[i] = getDfcSocket(*conf.servers[i]);
            if (connFds[i] != -1) {
                connectionFlag = true;
            }
        }
    }
    return connectionFlag;
}

int DfcUtils::getDfcSocket(const DfcServer& server) {
    int sockfd;
    struct sockaddr_in servAddr;
    struct timeval tv;
    
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = 5;  // 将超时时间从1秒增加到5秒
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to start socket:");
        exit(1);
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, 
                   (const char*)&tv, sizeof(struct timeval)) < 0) {
        perror("Unable to set timeout");
        exit(1);
    }
    
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(server.port);
    
    if (inet_pton(AF_INET, server.address.c_str(), &servAddr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(1);
    }
    
    if (connect(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        perror("Connection Failed");
        return -1;
    }
    
    return sockfd;
}

bool DfcUtils::commandBuilder(std::string& buffer, const std::string& format, 
                             const FileAttribute& fileAttr, const User& user, int flag) {
    std::string fileFolder = fileAttr.remote_file_folder;
    std::string fileName = fileAttr.remote_file_name;
    
    if (flag == LIST_FLAG) {
        fileFolder = (!fileFolder.empty()) ? fileFolder : "/";
        fileName = (!fileName.empty()) ? fileName : "NULL";
    } else if (flag == PUT_FLAG) {
        fileFolder = (!fileFolder.empty()) ? fileFolder : "/";
        if (fileName.empty()) return false;
        
        // For PUT command, ensure local file exists
        if (fileAttr.local_file_folder.length() > 1 && 
            !Utils::checkDirectoryExists(fileAttr.local_file_folder)) {
            std::cout << "<<< local directory doesn't exist: " << fileAttr.local_file_folder << std::endl;
            return false;
        }
        
        if (!Utils::checkFileExists(fileAttr.local_file_folder, fileAttr.local_file_name)) {
            std::cout << "<<< local file doesn't exist: " << fileAttr.local_file_folder 
                      << fileAttr.local_file_name << std::endl;
            return false;
        }
    } else if (flag == GET_FLAG) {
        fileFolder = (!fileFolder.empty()) ? fileFolder : "/";
        if (fileName.empty()) return false;
        
        // For GET command, ensure local directory exists for saving file
        if (!fileAttr.local_file_folder.empty() && 
            !Utils::checkDirectoryExists(fileAttr.local_file_folder)) {
            // Try to create the directory
            if (mkdir(fileAttr.local_file_folder.c_str(), 0755) == -1 && errno != EEXIST) {
                std::cout << "<<< Failed to create local directory: " << fileAttr.local_file_folder << std::endl;
                return false;
            }
        }
    } else if (flag == MKDIR_FLAG) {
        // 对于MKDIR命令，使用remote_file_folder作为要创建的文件夹路径
        fileFolder = (!fileAttr.remote_file_folder.empty()) ? fileAttr.remote_file_folder : "/";
        // 不需要检查fileName，因为我们已经在commandValidator中设置了dummy值
    }
    
    char tempBuffer[512];
    snprintf(tempBuffer, sizeof(tempBuffer), format.c_str(), flag, 
             user.username.c_str(), user.password.c_str(), 
             fileFolder.c_str(), fileName.c_str());
    
    buffer = std::string(tempBuffer);
    DEBUGSS("Command Built", buffer.c_str());
    return true;
}

void DfcUtils::commandHandler(std::vector<int>& connFds, int flag, 
                             const std::string& buffer, DfcConfig& conf) {
    FileAttribute fileAttr;
    std::string bufferToSend;
    bool builderFlag = false, connectionFlag;
    
    DEBUGS("Validating the command input");
    if (commandValidator(buffer, flag, fileAttr)) {
        DEBUGS("Building the command to be send");
        
        if (flag == LIST_FLAG || flag == GET_FLAG || flag == PUT_FLAG || flag == MKDIR_FLAG) {
            if (flag == GET_FLAG || flag == PUT_FLAG) {
                if (fileAttr.remote_file_name.empty()) {
                    fileAttr.remote_file_name = fileAttr.local_file_name;
                }
            }
            std::string templateStr;
            if (flag == LIST_FLAG) templateStr = LIST_TEMPLATE;
            else if (flag == GET_FLAG) templateStr = GET_TEMPLATE;
            else if (flag == PUT_FLAG) templateStr = PUT_TEMPLATE;
            else if (flag == MKDIR_FLAG) templateStr = MKDIR_TEMPLATE;
            builderFlag = commandBuilder(bufferToSend, templateStr, fileAttr, *conf.user, flag);
        }
        
        if (!builderFlag) {
            // 命令构建失败
        } else {
            DEBUGS("Creating Connections");
            connectionFlag = createConnections(connFds, conf);
            if (connectionFlag) {
                DEBUGS("Executing the command on remote servers");
                commandExec(connFds, bufferToSend, conf.server_count, fileAttr, flag, conf);
                DEBUGS("Tearing down connections");
                tearDownConnections(connFds, conf);
            } else {
                std::cout << "<<< Unable to Connect to any server" << std::endl;
            }
        }
    } else {
        std::cerr << "Failed to validate Command" << std::endl;
    }
}

bool DfcUtils::commandValidator(const std::string& buffer, int flag, FileAttribute& fileAttr) {
    int bufferLen = buffer.length();
    int charCount = Utils::getCountChar(buffer, ' ');
    
    if (flag == LIST_FLAG) {
        std::string tempBuffer = buffer;
        // Trim leading and trailing whitespace
        size_t start = tempBuffer.find_first_not_of(" \t\n\r");
        size_t end = tempBuffer.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) {
            // Empty or all whitespace
            tempBuffer = "/";
        } else {
            tempBuffer = tempBuffer.substr(start, end - start + 1);
            if (tempBuffer.empty()) {
                tempBuffer = "/";
            }
        }
        
        if (tempBuffer == "/") {
            bufferLen = 1;
        } else {
            bufferLen = tempBuffer.length();
        }
        
        Utils::extractFileNameAndFolder(tempBuffer, fileAttr, 1); // EXTRACT_REMOTE
    } else if (flag == PUT_FLAG || flag == GET_FLAG) {
        // 验证参数数量
        if (charCount != 1) {
            std::cerr << "Command not valid" << std::endl;
            return false;
        }
        
        // 获取两个参数
        std::string firstParam = Utils::getToken(buffer, " ", 0);
        std::string secondParam = Utils::getToken(buffer, " ", 1);
        
        if (firstParam.empty() || secondParam.empty()) {
            std::cerr << "Command not valid" << std::endl;
            return false;
        }
        
        if (flag == PUT_FLAG) {
            // PUT: firstParam = local path, secondParam = remote path
            Utils::extractFileNameAndFolder(firstParam, fileAttr, 0); // EXTRACT_LOCAL
            Utils::extractFileNameAndFolder(secondParam, fileAttr, 1); // EXTRACT_REMOTE
            
            // 验证本地文件存在
            std::string localDir = fileAttr.local_file_folder;
            // 处理根目录情况
            if (localDir == "/" || localDir.empty()) {
                localDir = "./";
            } else if (localDir.length() > 1 && localDir.back() != '/') {
                localDir += "/";
            }
            
            if (!Utils::checkDirectoryExists(localDir)) {
                std::cout << "<<< local directory doesn't exist: " << localDir << std::endl;
                return false;
            }
            
            if (!Utils::checkFileExists(localDir, fileAttr.local_file_name)) {
                std::cout << "<<< local file doesn't exist: " << localDir 
                          << fileAttr.local_file_name << std::endl;
                return false;
            }
        } else {
            // GET: firstParam = remote path, secondParam = local path
            Utils::extractFileNameAndFolder(firstParam, fileAttr, 1); // EXTRACT_REMOTE
            Utils::extractFileNameAndFolder(secondParam, fileAttr, 0); // EXTRACT_LOCAL
            
            // 验证本地保存目录存在
            std::string localDir = fileAttr.local_file_folder;
            if (localDir.empty()) {
                localDir = "./";
            } else if (localDir.length() > 1 && localDir.back() != '/') {
                localDir += "/";
            }
            
            if (!Utils::checkDirectoryExists(localDir)) {
                // Try to create the directory
                if (mkdir(localDir.c_str(), 0755) == -1 && errno != EEXIST) {
                    std::cout << "<<< Failed to create local directory: " << localDir << std::endl;
                    return false;
                }
            }
        }
    } else if (flag == MKDIR_FLAG) {
        if (charCount != 0) {
            std::cerr << "MKDIR command not valid" << std::endl;
            return false;
        }
        
        // 对于MKDIR命令，整个buffer就是要创建的文件夹路径
        std::string folderPath = buffer;
        // 确保路径以'/'结尾
        if (!folderPath.empty() && folderPath.back() != '/') {
            folderPath += '/';
        }
        fileAttr.remote_file_folder = folderPath;
        // 不需要文件名，设置一个非空值以通过commandBuilder检查
        fileAttr.remote_file_name = "dummy"; // 这个值不会被使用，只是为了通过检查
    } else {
        std::cerr << "<<< Unknown Command" << std::endl;
        return false;
    }
    
    return true;
}

bool DfcUtils::sendCommand(const std::vector<int>& connFds, const std::string& bufferToSend, 
                           int connCount) {
    bool sendFlag = true;
    
    for (int i = 0; i < connCount; i++) {
        if (connFds[i] == -1) continue;
        
        int payloadSize = bufferToSend.size();
        NetUtils::sendIntValueSocket(connFds[i], payloadSize);
        std::vector<unsigned char> payload(bufferToSend.begin(), bufferToSend.end());
        NetUtils::sendToSocket(connFds[i], payload);
    }
    
    return sendFlag;
}

void DfcUtils::sendFileSplits(int socket, const FileSplit& fileSplit, int mod, int serverIdx) {
    std::vector<unsigned char> payloadBuffer(MAX_SEG_SIZE, 0);
    
    for (int i = 0; i < CHUNKS_PER_SERVER; i++) {
        int filePiece = filePiecesMapping[mod][serverIdx][i];
        Split* split = fileSplit.splits[filePiece - 1].get();
        
        if (split) {
            std::cout << "DEBUG CLIENT: Sending split ID " << split->id << " to server " << serverIdx << std::endl;
            NetUtils::writeSplitToSocketAsStream(socket, *split);
            std::cout << "DEBUG CLIENT: Finished sending split ID " << split->id << " to server " << serverIdx << std::endl;
        } else {
            std::cout << "DEBUG CLIENT: No split for filePiece " << filePiece << " on server " << serverIdx << std::endl;
        }
    }
}

int DfcUtils::fetchRemoteFileInfo(const std::vector<int>& connFds, int connCount, 
                                  ServerChunksCollate& serverChunksCollate) {
    DEBUGSS("fetchRemoteFileInfo called with connCount", std::to_string(connCount).c_str());
    int mod = -1; // 明确声明并初始化 mod 变量

    // 接收所有服务器的文件信息
    for (int i = 0; i < connCount; i++) {
        if (connFds[i] == -1) continue;

        DEBUGSS("connFds status", (std::to_string(i) + ": " + std::to_string(connFds[i])).c_str());

        // 接收 hasData 标志
        int hasData;
        NetUtils::recvIntValueSocket(connFds[i], hasData);
        DEBUGSS("Received hasData from server", (std::to_string(i) + ": " + std::to_string(hasData)).c_str());

        // 接收 payloadSize（即使没有数据也要接收以保持同步）
        int payloadSize;
        NetUtils::recvIntValueSocket(connFds[i], payloadSize);
        DEBUGSS("Received payloadSize from server", (std::to_string(i) + ": " + std::to_string(payloadSize)).c_str());

        // 验证 payloadSize 的有效性
        if (payloadSize < 0 || payloadSize > MAX_SEG_SIZE) {
            DEBUGSS("Invalid payload size from server", std::to_string(i).c_str());
            // 接收占位符数据以保持通信同步
            std::vector<unsigned char> dummyPayload(payloadSize);
            if (payloadSize > 0) {
                NetUtils::recvFromSocket(connFds[i], dummyPayload);
            }
            continue;
        }

        // 接收实际负载数据
        std::vector<unsigned char> payload(payloadSize);
        NetUtils::recvFromSocket(connFds[i], payload);

        // 只有当有数据时才处理
        if (hasData > 0) {
            ServerChunksInfo serverChunksInfo;
            NetUtils::decodeServerChunksInfoFromBuffer(payload, serverChunksInfo);
            DEBUGSS("Received server chunks info from server", std::to_string(i).c_str());

            // 将信息插入聚合结构
            Utils::insertToServerChunksCollate(serverChunksCollate, serverChunksInfo);

            // 尝试从此服务器的信息中提取 mod 值
            if (mod == -1 && !serverChunksInfo.chunk_info.empty()) {
                const auto& chunkInfo = serverChunksInfo.chunk_info[0];
                if (chunkInfo.chunks.size() >= 2) {
                    DEBUGSS("Trying to find mod for chunks", 
                           (std::to_string(chunkInfo.chunks[0]) + "," + std::to_string(chunkInfo.chunks[1])).c_str());
                    for (int j = 0; j < 4; j++) {
                        if ((filePiecesMapping[j][i][0] == chunkInfo.chunks[0] && 
                             filePiecesMapping[j][i][1] == chunkInfo.chunks[1]) ||
                            (filePiecesMapping[j][i][0] == chunkInfo.chunks[1] && 
                             filePiecesMapping[j][i][1] == chunkInfo.chunks[0])) {
                            mod = j;
                            DEBUGSS("Found matching mod:", std::to_string(mod).c_str());
                            break;
                        }
                    }
                }
            }
        } else {
            // hasData == 0, 但仍然需要解码payload以获取chunks=0的信息
            ServerChunksInfo serverChunksInfo;
            NetUtils::decodeServerChunksInfoFromBuffer(payload, serverChunksInfo);
            // 当chunks=0时，不需要插入到聚合结构中
        }
    }

    // 如果无法确定mod，返回-1（调用者需要处理这种情况）
    return mod;
}

void DfcUtils::fetchRemoteSplits(std::vector<int>& connFds, int connCount, 
                                 FileSplit& fileSplit, int mod) {
    DEBUGS("Fetching remote splits from servers");
    
    // 初始化splits数组为4个元素（对应4个分片）
    fileSplit.split_count = 4;
    for (int i = 0; i < 4; i++) {
        fileSplit.splits[i] = std::make_unique<Split>();
    }
    
    // 为每个服务器构建需要获取的分片列表
    std::vector<std::vector<int>> remoteServers(connCount);
    for (int i = 0; i < connCount; i++) {
        if (connFds[i] == -1) continue;
        for (int j = 0; j < CHUNKS_PER_SERVER; j++) {
            int splitId = filePiecesMapping[mod][i][j];
            remoteServers[i].push_back(splitId);
        }
    }
    
    // 从每个服务器获取分片
    for (int i = 0; i < connCount; i++) {
        if (connFds[i] == -1) continue;
        std::cout << "DEBUG: Fetching from server " << i << std::endl;
        int socket = connFds[i];
        for (size_t j = 0; j < remoteServers[i].size(); j++) {  // 使用size_t避免警告
            int splitId = remoteServers[i][j];
            std::cout << "DEBUG: Requesting split ID " << splitId << " from server " << i << std::endl;
            NetUtils::sendIntValueSocket(socket, splitId);
            // 使用 splitId - 1 作为数组索引，因为分片ID是1-4，而数组索引是0-3
            if (splitId >= 1 && splitId <= 4) {
                NetUtils::writeSplitFromSocketAsStream(socket, *fileSplit.splits[splitId - 1]);
                std::cout << "DEBUG: Received split ID " << splitId << " from server " << i << std::endl;
                
                // 按照协议规范，在获取每个分片后发送RESET_SIG信号
                std::vector<unsigned char> resetSignal(1, RESET_SIG);
                NetUtils::sendToSocket(socket, resetSignal);
                std::cout << "DEBUG: Sent RESET_SIG to server " << i << std::endl;
            } else {
                std::cout << "ERROR: Invalid split ID " << splitId << std::endl;
            }
        }
    }
    
    DEBUGS("Finished fetching remote splits");
}

void DfcUtils::commandExec(std::vector<int>& connFds, const std::string& bufferToSend, 
                          int connCount, FileAttribute& attr, int flag, DfcConfig& conf) {
    bool sendFlag, errorFlag = false;  // 初始化errorFlag为false
    std::string filePath;
    int mod, c;
    FileSplit fileSplit;
    ServerChunksCollate serverChunksCollate;
    
    DEBUGS("Sending the command over to the servers");
    sendFlag = sendCommand(connFds, bufferToSend, connCount);
    
    if (sendFlag) {
        DEBUGS("Command sent over to the server successfully");
    } else {
        DEBUGSS("Unable to send command", strerror(errno));
        return;
    }
    
    for (int i = 0; i < connCount; i++) {
        if (connFds[i] == -1) continue;
        NetUtils::recvIntValueSocket(connFds[i], c);
        if (c == -1) {
            DEBUGS("Some Error has occured");
            errorFlag = true;
            NetUtils::fetchAndPrintError(connFds[i]);
        }
    }
    
    if (errorFlag) return;
    
    if (flag == LIST_FLAG) {
        DEBUGS("Fetching remote file(s) info from all the servers");
        mod = fetchRemoteFileInfo(connFds, connCount, serverChunksCollate);
        
        DEBUGS("Printing the file names and folders with status");
        getOutputListCommand(serverChunksCollate);
        
        fetchRemoteDirInfo(connFds, connCount);
        
        // 修复：发送RESET_SIG信号通知服务器可以关闭连接
        DEBUGS("Sending RESET_SIG to servers after LIST command");
        NetUtils::sendSignal(connFds, RESET_SIG);
        
    } else if (flag == GET_FLAG) {
        DEBUGS("Fetching remote file(s) info from all the servers");
        mod = fetchRemoteFileInfo(connFds, connCount, serverChunksCollate);
        
        if (mod < 0) {
            // 如果无法从服务器信息推断mod值，直接计算文件名的mod值
            std::string fileName = "/" + attr.remote_file_name;
            mod = Utils::getMd5SumHashMod(fileName);
            DEBUGSS("Calculated mod value directly from filename", std::to_string(mod).c_str());
        }
        
        // 检查是否有文件信息
        if (serverChunksCollate.num_files == 0) {
            std::cout << "<<< File not found on any server" << std::endl;
            DEBUGS("Sending RESET_SIG to servers");
            NetUtils::sendSignal(connFds, RESET_SIG);
            return;
        }
        
        DEBUGS("Checking whether the file is complete");
        if (!Utils::checkComplete(serverChunksCollate.chunks[0])) {
            std::cout << "<<< File is incomplete" << std::endl;
            DEBUGS("Sending REST_SIG to server");
            NetUtils::sendSignal(connFds, RESET_SIG);
        } else {
            DEBUGS("File can can be fetched");
            DEBUGS("Sending PROCEED_SIG to server");
            NetUtils::sendSignal(connFds, PROCEED_SIG);
            
            DEBUGS("Fetching remote splits from the server");
            fetchRemoteSplits(connFds, connCount, fileSplit, mod);
            
            DEBUGS("Decrypting the file splits");
            Utils::encryptDecryptFileSplit(fileSplit, conf.user->password);
            DEBUGS("Combining all the splits and writing into the file");
            
            combineFileFromPieces(attr, fileSplit);
            Utils::freeFileSplit(fileSplit);
        }
        
    } else if (flag == PUT_FLAG) {
        DEBUGS("Getting mod value on file-content");
        filePath = attr.local_file_folder + attr.local_file_name;
        mod = Utils::getMd5SumHashMod(filePath);
        
        DEBUGS("Splitting file into pieces");
        splitFileToPieces(filePath, fileSplit);  // 移除connCount参数
        
        DEBUGS("Encrypting the file splits");
        Utils::encryptDecryptFileSplit(fileSplit, conf.user->password);
        DEBUGS("Sending splits to servers");
        
        // 发送分片到服务器
        for (int i = 0; i < connCount; i++) {
            if (connFds[i] != -1) {
                sendFileSplits(connFds[i], fileSplit, mod, i);
            }
        }
        DEBUGS("Splits sent to servers");
        
        // 等待所有服务器的确认响应
        bool putSuccess = true;
        for (int i = 0; i < connCount; i++) {
            if (connFds[i] != -1) {
                int response;
                NetUtils::recvIntValueSocket(connFds[i], response);
                if (response != 1) {
                    putSuccess = false;
                    DEBUGSS("Server response error", std::to_string(i).c_str());
                }
            }
        }
        
        if (putSuccess) {
            std::cout << "<<< File uploaded successfully!" << std::endl;
        } else {
            std::cout << "<<< File upload failed!" << std::endl;
        }
        
        Utils::freeFileSplit(fileSplit);
    } else if (flag == MKDIR_FLAG) {
        // MKDIR命令处理 - 仅需发送命令，无需额外操作
        DEBUGS("MKDIR command executed, no additional processing needed");
    }
}

void DfcUtils::fetchRemoteDirInfo(const std::vector<int>& connFds, int connCount) {
    std::set<std::string> uniqueFolders;
    
    for (int i = 0; i < connCount; i++) {
        if (connFds[i] == -1) continue;
        
        int payloadSize;
        NetUtils::recvIntValueSocket(connFds[i], payloadSize);
        DEBUGSS("Received folder payloadSize from server", (std::to_string(i) + ": " + std::to_string(payloadSize)).c_str());
        
        // 检查payloadSize是否有效
        if (payloadSize < 0 || payloadSize > MAX_SEG_SIZE) {
            DEBUGSS("Invalid folder payload size from server", std::to_string(i).c_str());
            DEBUGSS("Folder payload size:", std::to_string(payloadSize).c_str());
            continue;
        }
        
        if (payloadSize > 0) {
            std::vector<unsigned char> payload(payloadSize);
            NetUtils::recvFromSocket(connFds[i], payload);
            
            // 将接收到的数据转换为字符串并按行分割
            std::string folderData(reinterpret_cast<char*>(payload.data()), payload.size());
            std::stringstream ss(folderData);
            std::string line;
            
            while (std::getline(ss, line, '\n')) {
                if (!line.empty()) {
                    uniqueFolders.insert(line);
                }
            }
        }
        // 当 payloadSize == 0 时，不接收任何数据
    }
    
    // 输出去重后的目录列表
    for (const auto& folder : uniqueFolders) {
        std::cout << folder << std::endl;
    }
}

void DfcUtils::getOutputListCommand(const ServerChunksCollate& serverChunksCollate) {
    for (int i = 0; i < serverChunksCollate.num_files; i++) {
        std::cout << std::string(serverChunksCollate.file_names[i].data());
        if (Utils::checkComplete(serverChunksCollate.chunks[i])) {
            std::cout << std::endl;
        } else {
            std::cout << " [INCOMPLETE]" << std::endl;
        }
    }
}

bool DfcUtils::authConnections(const std::vector<int>& connFds, const DfcConfig& conf) {
    bool ans = false;
    std::string buffer, response;
    
    int nBytes = NetUtils::encodeUserStruct(buffer, *conf.user);
    
    for (int i = 0; i < conf.server_count; i++) {
        int socket = connFds[i];
        
        if (socket != -1) {
            ans = true;
            std::vector<unsigned char> payload(buffer.begin(), buffer.end());
            if (NetUtils::sendToSocket(socket, payload) != nBytes) {
                std::cerr << "Failed to send auth message" << std::endl;
                return false;
            }
            
            std::vector<unsigned char> respBuffer(MAX_SEG_SIZE);
            int rBytes = NetUtils::recvFromSocket(socket, respBuffer);
            if (rBytes <= 0) {
                std::cerr << "Failed to recv auth message" << std::endl;
                return false;
            }
            
            response.assign(respBuffer.begin(), respBuffer.begin() + rBytes);
            if (response != AUTH_OK) return false;
        }
    }
    
    return ans;
}

void DfcUtils::readDfcConf(const std::string& filePath, DfcConfig& conf) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        perror("DFC => Error in opening config file: ");
        exit(1);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        
        if (line.find(DFC_SERVER_CONF) != std::string::npos) {
            insertServerConf(line, conf);
        } else if (line.find(DFC_USERNAME_CONF) != std::string::npos) {
            insertUserConf(line, conf, DFC_USERNAME_DELIM, USERNAME_FLAG);
        } else if (line.find(DFC_PASSWORD_CONF) != std::string::npos) {
            insertUserConf(line, conf, DFC_PASSWORD_DELIM, PASSWORD_FLAG);
        }
    }
    
    file.close();
}

bool DfcUtils::checkServerStruct(std::unique_ptr<DfcServer>& server) {
    if (!server) {
        server = std::make_unique<DfcServer>();
        return false;
    }
    return true;
}

void DfcUtils::insertServerConf(const std::string& line, DfcConfig& conf) {
    std::string tempLine = Utils::getSubstringAfter(line, " ");
    std::string name = Utils::getToken(tempLine, " ", 0);
    std::string addressPort = Utils::getToken(tempLine, " ", 1);
    std::string address = Utils::getToken(addressPort, ":", 0);
    std::string portStr = Utils::getSubstringAfter(addressPort, ":");
    
    int i = conf.server_count++;
    checkServerStruct(conf.servers[i]);
    
    conf.servers[i]->name = name;
    conf.servers[i]->address = address;
    conf.servers[i]->port = std::stoi(portStr);
}

void DfcUtils::insertUserConf(const std::string& line, DfcConfig& conf, 
                             const std::string& delim, int flag) {
    if (!conf.user) {
        conf.user = std::make_unique<User>();
    }
    
    std::string ptr = Utils::getSubstringAfter(line, delim);
    if (flag == PASSWORD_FLAG) {
        conf.user->password = ptr;
    } else {
        conf.user->username = ptr;
    }
}

bool DfcUtils::splitFileToPieces(const std::string& filePath, FileSplit& fileSplit) {  // 移除未使用的参数n
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filePath << std::endl;
        return false;
    }
    
    file.seekg(0, std::ios::end);
    long long fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    fileSplit.file_name = filePath;
    
    // 总是创建4个分片，以匹配filePiecesMapping的设计
    const int totalChunks = 4;
    long long splitSize = fileSize / totalChunks;
    long long remSize = fileSize % totalChunks;
    
    fileSplit.split_count = totalChunks;
    for (int i = 0; i < totalChunks; i++) {
        fileSplit.splits[i] = std::make_unique<Split>();
        auto& split = *fileSplit.splits[i];
        
        split.id = i + 1;
        split.content_length = (i != totalChunks - 1) ? splitSize : splitSize + remSize;
        split.content.resize(split.content_length);
        
        file.read(reinterpret_cast<char*>(split.content.data()), split.content_length);
    }
    
    file.close();
    return true;
}

bool DfcUtils::combineFileFromPieces(const FileAttribute& fileAttr, const FileSplit& fileSplit) {
    std::string fileName = fileAttr.local_file_folder + fileAttr.local_file_name;
    DEBUGSS("Writing to file", fileName.c_str());
    
    std::ofstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "<<< Unable to open file to write: " << strerror(errno) << std::endl;
        return false;
    }
    
    for (int i = 0; i < fileSplit.split_count; i++) {
        if (fileSplit.splits[i]) {
            const auto& split = *fileSplit.splits[i];
            file.write(reinterpret_cast<const char*>(split.content.data()), split.content_length);
        }
    }
    
    file.close();
    return true;
}

void DfcUtils::printDfcConf(const DfcConfig& conf) {
    for (int i = 0; i < conf.server_count; i++) {
        if (conf.servers[i]) {
            std::cerr << "DEBUG: Name:" << conf.servers[i]->name 
                      << " Address:" << conf.servers[i]->address 
                      << " Port:" << conf.servers[i]->port << std::endl;
        }
    }
}

void DfcUtils::freeDfcConf(DfcConfig& conf) {
    conf.user.reset();
    for (int i = 0; i < conf.server_count; i++) {
        conf.servers[i].reset();
    }
    conf.server_count = 0;
}

void DfcUtils::freeDfcServer(DfcServer& server) {
    server.name.clear();
    server.address.clear();
    server.port = 0;
}