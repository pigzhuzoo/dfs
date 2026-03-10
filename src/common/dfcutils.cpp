#include "dfcutils.hpp"
#include "thread_pool.hpp"
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
#include <thread>
#include <mutex>
#include <future>
#include <atomic>

namespace {
    constexpr size_t PARALLEL_THRESHOLD = 256 * 1024;
    
    bool shouldUseParallel(size_t fileSize) {
        return fileSize >= PARALLEL_THRESHOLD;
    }
}

const int DfcUtils::filePiecesMapping[4][4][2] = {
    { { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 1 } },
    { { 4, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 } },
    { { 3, 4 }, { 4, 1 }, { 1, 2 }, { 2, 3 } },
    { { 2, 3 }, { 3, 4 }, { 4, 1 }, { 1, 2 } }
};

void DfcUtils::setupConnections(std::vector<int>& connFds, const DfcConfig& conf) {
    connFds.resize(conf.server_count, -1);
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
    std::atomic<bool> connectionFlag(false);
    auto& pool = ThreadPool::getInstance();
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < conf.server_count; i++) {
        if (conf.servers[i]) {
            futures.push_back(pool.enqueue([&connFds, &conf, i, &connectionFlag]() {
                int fd = getDfcSocket(*conf.servers[i]);
                connFds[i] = fd;
                if (fd != -1) {
                    connectionFlag = true;
                }
            }));
        }
    }
    
    for (auto& f : futures) {
        f.wait();
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
    (void)mod;  // Mark as intentionally unused
    (void)serverIdx;  // Mark as intentionally unused
    int objectCount = fileSplit.object_count;
    
    NetUtils::sendIntValueSocket(socket, objectCount);
    
    for (int i = 0; i < objectCount; i++) {
        if (i < static_cast<int>(fileSplit.objects.size()) && fileSplit.objects[i]) {
            NetUtils::sendIntValueSocket(socket, fileSplit.objects[i]->id);
            NetUtils::writeSplitToSocketAsStream(socket, *fileSplit.objects[i]);
        }
    }
    
    std::vector<unsigned char> endSignal(1, RESET_SIG);
    NetUtils::sendToSocket(socket, endSignal);
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

            if (mod == -1 && !serverChunksInfo.chunk_info.empty()) {
                const auto& chunkInfo = serverChunksInfo.chunk_info[0];
                if (chunkInfo.chunks.size() >= 1) {
                    DEBUGSS("Trying to find mod for chunks", 
                           (std::to_string(chunkInfo.chunks[0]) + ", size=" + std::to_string(chunkInfo.chunks.size())).c_str());
                    for (int j = 0; j < 4; j++) {
                        if (chunkInfo.chunks.size() >= 2) {
                            if ((filePiecesMapping[j][i][0] == chunkInfo.chunks[0] && 
                                 filePiecesMapping[j][i][1] == chunkInfo.chunks[1]) ||
                                (filePiecesMapping[j][i][0] == chunkInfo.chunks[1] && 
                                 filePiecesMapping[j][i][1] == chunkInfo.chunks[0])) {
                                mod = j;
                                DEBUGSS("Found matching mod:", std::to_string(mod).c_str());
                                break;
                            }
                        } else {
                            if (filePiecesMapping[j][i][0] == chunkInfo.chunks[0] ||
                                filePiecesMapping[j][i][1] == chunkInfo.chunks[0]) {
                                mod = j;
                                DEBUGSS("Found matching mod (single chunk):", std::to_string(mod).c_str());
                                break;
                            }
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
                                 FileSplit& fileSplit, int mod, size_t fileSize) {
    (void)mod;  // Mark as intentionally unused
    int estimatedObjectCount = 1;
    if (fileSize > 0) {
        estimatedObjectCount = Utils::calculateObjectCount(fileSize, DEFAULT_OBJECT_SIZE);
    }
    
    int maxObjects = std::max(estimatedObjectCount, 16);
    
    fileSplit.objects.clear();
    fileSplit.objects.reserve(maxObjects);
    fileSplit.object_count = 0;
    fileSplit.object_size = DEFAULT_OBJECT_SIZE;
    
    DEBUGS("Fetching remote objects (serial)");
    
    for (int serverIdx = 0; serverIdx < connCount; serverIdx++) {
        if (connFds[serverIdx] == -1) continue;
        
        int socket = connFds[serverIdx];
        
        for (int objId = 0; objId < maxObjects; objId++) {
            while (objId >= static_cast<int>(fileSplit.objects.size())) {
                fileSplit.objects.push_back(std::make_unique<Split>());
            }
            
            NetUtils::sendIntValueSocket(socket, objId);
            NetUtils::writeSplitFromSocketAsStream(socket, *fileSplit.objects[objId]);
            
            if (fileSplit.objects[objId]->content_length == 0) {
                DEBUGSS("Object not found on server, stopping at object", std::to_string(objId).c_str());
                fileSplit.objects.pop_back();
                break;
            }
            
            fileSplit.objects[objId]->offset = static_cast<size_t>(objId) * DEFAULT_OBJECT_SIZE;
            fileSplit.object_count = objId + 1;
            
            std::vector<unsigned char> resetSignal(1, RESET_SIG);
            NetUtils::sendToSocket(socket, resetSignal);
        }
        break;
    }
    
    if (fileSplit.object_count > 0) {
        DEBUGSS("All objects fetched successfully, count:", std::to_string(fileSplit.object_count).c_str());
    } else {
        DEBUGS("No objects fetched!");
    }
    
    DEBUGS("Finished fetching remote objects");
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
            std::string fileName = "/" + attr.remote_file_name;
            mod = Utils::getMd5SumHashMod(fileName);
            DEBUGSS("Calculated mod value directly from filename", std::to_string(mod).c_str());
        }
        
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
            DEBUGS("File can be fetched");
            DEBUGS("Sending PROCEED_SIG to server");
            NetUtils::sendSignal(connFds, PROCEED_SIG);
            
            DEBUGS("Fetching remote objects from the server");
            size_t estimatedFileSize = fileSplit.file_size;
            fetchRemoteSplits(connFds, connCount, fileSplit, mod, estimatedFileSize);
            
            DEBUGS("Decrypting the file objects");
            Utils::encryptDecryptFileSplit(fileSplit, conf.user->password, conf.encryption_type, false);
            DEBUGS("Combining all the objects and writing into the file");
            
            combineFileFromPieces(attr, fileSplit);
            Utils::freeFileSplit(fileSplit);
        }
        
    } else if (flag == PUT_FLAG) {
        DEBUGS("Getting mod value on file-content");
        filePath = attr.local_file_folder + attr.local_file_name;
        mod = Utils::getMd5SumHashMod(filePath);
        
        DEBUGS("Splitting file into objects (Ceph style)");
        splitFileToPieces(filePath, fileSplit);
        
        size_t fileSize = fileSplit.file_size;
        
        DEBUGSS("File size", std::to_string(fileSize).c_str());
        DEBUGSS("Object count", std::to_string(fileSplit.object_count).c_str());
        DEBUGSS("Object size", std::to_string(fileSplit.object_size).c_str());
        
        DEBUGS("Encrypting the file objects");
        Utils::encryptDecryptFileSplit(fileSplit, conf.user->password, conf.encryption_type, true);
        
        if (shouldUseParallel(fileSize)) {
            DEBUGS("Sending objects to servers (parallel, thread pool)");
            auto& pool = ThreadPool::getInstance();
            std::vector<std::future<void>> sendFutures;
            for (int i = 0; i < connCount; i++) {
                if (connFds[i] != -1) {
                    sendFutures.push_back(pool.enqueue([&connFds, &fileSplit, mod, i]() {
                        sendFileSplits(connFds[i], fileSplit, mod, i);
                    }));
                }
            }
            for (auto& f : sendFutures) {
                f.wait();
            }
        } else {
            DEBUGS("Sending objects to servers (serial)");
            for (int i = 0; i < connCount; i++) {
                if (connFds[i] != -1) {
                    sendFileSplits(connFds[i], fileSplit, mod, i);
                }
            }
        }
        DEBUGS("Objects sent to servers");
        
        std::atomic<bool> putSuccess(true);
        auto& pool = ThreadPool::getInstance();
        std::vector<std::future<void>> recvFutures;
        for (int i = 0; i < connCount; i++) {
            if (connFds[i] != -1) {
                recvFutures.push_back(pool.enqueue([&connFds, &putSuccess, i]() {
                    int response;
                    NetUtils::recvIntValueSocket(connFds[i], response);
                    if (response != 1) {
                        putSuccess = false;
                        DEBUGSS("Server response error", std::to_string(i).c_str());
                    }
                }));
            }
        }
        for (auto& f : recvFutures) {
            f.wait();
        }
        
        if (putSuccess) {
            std::cout << "<<< File uploaded successfully!" << std::endl;
            std::cout << "    File size: " << fileSize << " bytes" << std::endl;
            std::cout << "    Objects: " << fileSplit.object_count << std::endl;
            std::cout << "    Object size: " << fileSplit.object_size << " bytes" << std::endl;
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
        } else if (line.find("EncryptionType") != std::string::npos) {
            std::string typeStr = Utils::getSubstringAfter(line, "EncryptionType: ");
            if (!typeStr.empty()) {
                if (typeStr == "AES_256_GCM" || typeStr == "0") {
                    conf.encryption_type = EncryptionType::AES_256_GCM;
                } else if (typeStr == "AES_256_ECB" || typeStr == "1") {
                    conf.encryption_type = EncryptionType::AES_256_ECB;
                } else if (typeStr == "AES_256_CBC" || typeStr == "2") {
                    conf.encryption_type = EncryptionType::AES_256_CBC;
                } else if (typeStr == "AES_256_CFB" || typeStr == "3") {
                    conf.encryption_type = EncryptionType::AES_256_CFB;
                } else if (typeStr == "AES_256_OFB" || typeStr == "4") {
                    conf.encryption_type = EncryptionType::AES_256_OFB;
                } else if (typeStr == "AES_256_CTR" || typeStr == "5") {
                    conf.encryption_type = EncryptionType::AES_256_CTR;
                } else if (typeStr == "SM4_ECB" || typeStr == "6") {
                    conf.encryption_type = EncryptionType::SM4_ECB;
                } else if (typeStr == "SM4_CBC" || typeStr == "7") {
                    conf.encryption_type = EncryptionType::SM4_CBC;
                } else if (typeStr == "SM4_CTR" || typeStr == "8") {
                    conf.encryption_type = EncryptionType::SM4_CTR;
                } else if (typeStr == "RSA_OAEP" || typeStr == "9") {
                    conf.encryption_type = EncryptionType::RSA_OAEP;
                } else if (typeStr == "AES_256_FPGA" || typeStr == "10") {
                    conf.encryption_type = EncryptionType::AES_256_FPGA;
                } else {
                    conf.encryption_type = EncryptionType::AES_256_ECB;
                }
                dfs::crypto::EncryptionAlgorithm crypto_algo;
                switch (conf.encryption_type) {
                    case EncryptionType::AES_256_GCM:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_GCM;
                        break;
                    case EncryptionType::AES_256_ECB:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_ECB;
                        break;
                    case EncryptionType::AES_256_CBC:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_CBC;
                        break;
                    case EncryptionType::AES_256_CFB:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_CFB;
                        break;
                    case EncryptionType::AES_256_OFB:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_OFB;
                        break;
                    case EncryptionType::AES_256_CTR:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_CTR;
                        break;
                    case EncryptionType::SM4_ECB:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::SM4_ECB;
                        break;
                    case EncryptionType::SM4_CBC:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::SM4_CBC;
                        break;
                    case EncryptionType::SM4_CTR:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::SM4_CTR;
                        break;
                    case EncryptionType::RSA_OAEP:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::RSA_OAEP;
                        break;
                    case EncryptionType::AES_256_FPGA:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_FPGA;
                        break;
                    default:
                        crypto_algo = dfs::crypto::EncryptionAlgorithm::AES_256_ECB;
                        break;
                }
                DEBUGSS("Encryption type set to:", dfs::crypto::CryptoUtils::getAlgorithmName(crypto_algo).c_str());
            }
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

bool DfcUtils::splitFileToPieces(const std::string& filePath, FileSplit& fileSplit) {
    return Utils::splitFileToObjects(filePath, fileSplit);
}

bool DfcUtils::combineFileFromPieces(const FileAttribute& fileAttr, const FileSplit& fileSplit) {
    std::string fileName = fileAttr.local_file_folder;
    if (fileName.empty() || fileName.back() != '/') {
        fileName += "/";
    }
    fileName += fileAttr.local_file_name;
    DEBUGSS("Writing to file", fileName.c_str());
    
    return Utils::combineFileFromObjects(fileName, fileSplit);
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