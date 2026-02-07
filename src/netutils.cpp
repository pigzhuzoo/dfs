#include "../include/netutils.hpp"
#include "../include/logger.hpp"
#include <cstring>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <arpa/inet.h>  // 添加网络字节序函数头文件

void NetUtils::fetchAndPrintError(int socket) {
    int payloadSize;
    recvIntValueSocket(socket, payloadSize);
    
    std::vector<unsigned char> payload(payloadSize + 1);
    recvFromSocket(socket, payload);
    payload[payloadSize] = '\0';
    
    std::cout << "<<< Error Message: " << reinterpret_cast<char*>(payload.data()) << std::endl;
}

void NetUtils::sendIntValueSocket(int socket, int value) {
    std::vector<unsigned char> payload(INT_SIZE);
    encodeIntToUchar(payload, value);
    sendToSocket(socket, payload);
}

void NetUtils::recvIntValueSocket(int socket, int& value) {
    std::vector<unsigned char> payload(INT_SIZE, 0); // 初始化为0
    recvFromSocket(socket, payload);
    decodeIntFromUchar(payload, value);
}

void NetUtils::encodeIntToUchar(std::vector<unsigned char>& buffer, int n) {
    if (buffer.size() < INT_SIZE) {
        buffer.resize(INT_SIZE);
    }
    // 使用htonl确保网络字节序（大端序）
    uint32_t network_order = htonl(static_cast<uint32_t>(n));
    buffer[0] = (network_order >> 24) & 0xFF;
    buffer[1] = (network_order >> 16) & 0xFF;
    buffer[2] = (network_order >> 8) & 0xFF;
    buffer[3] = network_order & 0xFF;
}

void NetUtils::decodeIntFromUchar(const std::vector<unsigned char>& buffer, int& n) {
    // 从网络字节序重建整数
    uint32_t network_order = (static_cast<uint32_t>(buffer[0]) << 24) |
                            (static_cast<uint32_t>(buffer[1]) << 16) |
                            (static_cast<uint32_t>(buffer[2]) << 8) |
                            static_cast<uint32_t>(buffer[3]);
    // 使用ntohl转换为主机字节序
    n = static_cast<int>(ntohl(network_order));
}

int NetUtils::encodeUserStruct(std::string& buffer, const User& user) {
    char tempBuffer[256];
    int nBytes = sprintf(tempBuffer, AUTH_TEMPLATE, AUTH_FLAG, 
                        user.username.c_str(), user.password.c_str());
    buffer = std::string(tempBuffer);
    
    if (nBytes < 0) {
        perror("Failed to Encode User Struct");
        exit(1);
    }
    return nBytes;
}

void NetUtils::decodeUserStruct(const std::string& buffer, User& user) {
    int flag;
    char username[MAX_CHAR_BUFF], password[MAX_CHAR_BUFF];
    
    if (sscanf(buffer.c_str(), AUTH_TEMPLATE, &flag, username, password) <= 0) {
        perror("Failed to decode user struct string");
        exit(1);
    }
    
    user.username = std::string(username);
    user.password = std::string(password);
    
    if (flag != AUTH_FLAG) {
        std::cerr << "Failed to decode User Struct" << std::endl;
        exit(1);
    }
}

int NetUtils::sendToSocket(int socket, const std::vector<unsigned char>& payload) {
    int sBytes = 0;
    int sizeOfPayload = payload.size();
    
    while (sBytes != sizeOfPayload) {
        int result = send(socket, payload.data() + sBytes, sizeOfPayload - sBytes, 0);
        if (result < 0) {
            perror("Unable to send entire payload via socket");
            exit(1);
        }
        sBytes += result;
    }
    
    return sBytes;
}

int NetUtils::recvFromSocket(int socket, std::vector<unsigned char>& payload) {
    int rBytes = 0;
    int sizeOfPayload = payload.size();
    
    // 初始化缓冲区为0
    std::fill(payload.begin(), payload.end(), 0);
    
    while (rBytes != sizeOfPayload) {
        int result = recv(socket, payload.data() + rBytes, sizeOfPayload - rBytes, 0);
        if (result < 0) {
            // 处理超时错误（EAGAIN/EWOULDBLOCK），继续重试
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 继续循环，稍后重试
                continue;
            }
            DEBUGSS("Unable to receive entire payload via socket", strerror(errno));
            exit(1);
        }
        
        if (result == 0) {
            // 连接被对端关闭
            if (rBytes == sizeOfPayload) {
                // 已经收到了完整的数据，连接关闭是正常的
                break;
            } else {
                // 无法接收完整数据
                DEBUGSS("Connection closed by peer before receiving complete payload", "");
                exit(1);
            }
        }
        rBytes += result;
    }
    
    return rBytes;
}

void NetUtils::sendSignal(const std::vector<int>& connFds, unsigned char signal) {
    for (int socket : connFds) {
        if (socket != -1) {
            std::vector<unsigned char> payload(1, signal);
            sendToSocket(socket, payload);
        }
    }
}

void NetUtils::recvSignal(int socket, unsigned char& payload) {
    std::vector<unsigned char> buffer(1);
    recvFromSocket(socket, buffer);
    payload = buffer[0];
}

void NetUtils::encodeServerChunksInfoToBuffer(std::vector<unsigned char>& buffer, 
                                             const ServerChunksInfo& serverChunksInfo) {
    // 先调整缓冲区大小
    buffer.resize(INT_SIZE + serverChunksInfo.chunks * CHUNK_INFO_STRUCT_SIZE);
    
    // 首四个字节包含chunks数量
    encodeIntToUchar(buffer, serverChunksInfo.chunks);
    
    for (int i = 0; i < serverChunksInfo.chunks; i++) {
        std::vector<unsigned char> chunkBuffer(CHUNK_INFO_STRUCT_SIZE);
        encodeChunkInfoToBuffer(chunkBuffer, serverChunksInfo.chunk_info[i]);
        std::copy(chunkBuffer.begin(), chunkBuffer.end(), 
                 buffer.begin() + INT_SIZE + i * CHUNK_INFO_STRUCT_SIZE);
    }
}

void NetUtils::decodeServerChunksInfoFromBuffer(const std::vector<unsigned char>& buffer, 
                                               ServerChunksInfo& serverChunksInfo) {
    decodeIntFromUchar(buffer, serverChunksInfo.chunks);
    
    serverChunksInfo.chunk_info.resize(serverChunksInfo.chunks);
    
    for (int i = 0; i < serverChunksInfo.chunks; i++) {
        std::vector<unsigned char> chunkBuffer(buffer.begin() + INT_SIZE + i * CHUNK_INFO_STRUCT_SIZE,
                                              buffer.begin() + INT_SIZE + (i + 1) * CHUNK_INFO_STRUCT_SIZE);
        decodeChunkInfoFromBuffer(chunkBuffer, serverChunksInfo.chunk_info[i]);
    }
}

void NetUtils::encodeChunkInfoToBuffer(std::vector<unsigned char>& buffer, 
                                      const ChunkInfo& chunkInfo) {
    // 复制文件名
    strncpy(reinterpret_cast<char*>(buffer.data()), chunkInfo.file_name.c_str(), MAX_CHAR_BUFF);
    
    // 编码块编号
    for (int i = 0; i < CHUNKS_PER_SERVER; i++) {
        std::vector<unsigned char> intBuffer(INT_SIZE);
        encodeIntToUchar(intBuffer, chunkInfo.chunks[i]);
        std::copy(intBuffer.begin(), intBuffer.end(), 
                 buffer.begin() + MAX_CHAR_BUFF + i * INT_SIZE);
    }
}

void NetUtils::decodeChunkInfoFromBuffer(const std::vector<unsigned char>& buffer, 
                                        ChunkInfo& chunkInfo) {
    // 解码文件名
    chunkInfo.file_name = std::string(reinterpret_cast<const char*>(buffer.data()));
    
    // 解码块编号
    for (int i = 0; i < CHUNKS_PER_SERVER; i++) {
        std::vector<unsigned char> intBuffer(buffer.begin() + MAX_CHAR_BUFF + i * INT_SIZE,
                                            buffer.begin() + MAX_CHAR_BUFF + (i + 1) * INT_SIZE);
        decodeIntFromUchar(intBuffer, chunkInfo.chunks[i]);
    }
}

void NetUtils::writeSplitToSocketAsStream(int socket, const Split& split) {
    // 1字节标志，4字节split_id，4字节content_length
    std::vector<unsigned char> headerBuffer(9, 0);
    
    headerBuffer[0] = INITIAL_WRITE_FLAG;
    
    std::vector<unsigned char> idBuffer(INT_SIZE);
    encodeIntToUchar(idBuffer, split.id);
    std::copy(idBuffer.begin(), idBuffer.end(), headerBuffer.begin() + 1);
    
    std::vector<unsigned char> lengthBuffer(INT_SIZE);
    encodeIntToUchar(lengthBuffer, static_cast<int>(split.content_length));
    std::copy(lengthBuffer.begin(), lengthBuffer.end(), headerBuffer.begin() + 5);
    
    // 先发送9字节头部
    sendToSocket(socket, headerBuffer);
    
    // 然后发送内容
    if (split.content_length > 0) {
        std::vector<unsigned char> contentBuffer(split.content.begin(), split.content.end());
        sendToSocket(socket, contentBuffer);
    }
    
    std::cout << "DEBUG CLIENT: Sending split ID: " << split.id << ", Content length: " << split.content_length << std::endl;
}

void NetUtils::writeSplitFromSocketAsStream(int socket, Split& split) {
    log_debug("Starting writeSplitFromSocketAsStream - waiting for 9-byte header");
    
    // 使用向量接收9字节头部：1字节标志 + 4字节分片ID + 4字节内容长度
    std::vector<unsigned char> headerBuffer(9);
    int bytesReceived = recvFromSocket(socket, headerBuffer);
    log_debug("Received " + std::to_string(bytesReceived) + " bytes for header");
    
    if (bytesReceived != 9) {
        log_error("Failed to receive complete header. Expected 9 bytes, got " + 
                 std::to_string(bytesReceived));
        throw std::runtime_error("Connection closed by peer before receiving complete payload");
    }
    
    // 解析头部
    unsigned char flag = headerBuffer[0];
    log_debug("Received flag: " + std::to_string(flag));
    
    if (flag != INITIAL_WRITE_FLAG) {
        log_error("Invalid flag received: " + std::to_string(flag) + 
                 ", expected: " + std::to_string(INITIAL_WRITE_FLAG));
        throw std::runtime_error("Invalid flag in split header");
    }
    
    // 解析分片ID（4字节）
    int splitId;
    decodeIntFromUchar(std::vector<unsigned char>(headerBuffer.begin() + 1, headerBuffer.begin() + 5), splitId);
    log_debug("Received split ID: " + std::to_string(splitId));
    
    // 解析内容长度（4字节）
    int contentLength;
    decodeIntFromUchar(std::vector<unsigned char>(headerBuffer.begin() + 5, headerBuffer.begin() + 9), contentLength);
    log_debug("Received content length: " + std::to_string(contentLength));
    
    if (contentLength < 0 || contentLength > MAX_SEG_SIZE) {
        log_error("Invalid content length: " + std::to_string(contentLength));
        throw std::runtime_error("Invalid content length in split header");
    }
    
    // 接收内容
    log_debug("Preparing to receive " + std::to_string(contentLength) + " bytes of content");
    split.content.resize(contentLength);
    if (contentLength > 0) {
        int contentBytesReceived = recvFromSocket(socket, split.content);
        log_debug("Received " + std::to_string(contentBytesReceived) + " bytes of content");
        if (contentBytesReceived != contentLength) {
            log_error("Failed to receive complete content. Expected " + 
                     std::to_string(contentLength) + " bytes, got " + 
                     std::to_string(contentBytesReceived));
            throw std::runtime_error("Connection closed by peer before receiving complete payload");
        }
    }
    
    split.id = splitId;
    split.content_length = contentLength;
    log_debug("Successfully received split with ID " + std::to_string(splitId) + 
             " and length " + std::to_string(contentLength));
}

// 旧版兼容函数实现
void NetUtils::encodeSplitToBuffer(std::vector<unsigned char>& buffer, const Split& split) {
    encodeIntToUchar(buffer, split.id);
    encodeIntToUchar(buffer, static_cast<int>(split.content_length));
    
    if (buffer.size() < 9 + split.content_length) {
        buffer.resize(9 + split.content_length);
    }
    
    std::copy(split.content.begin(), split.content.end(), buffer.begin() + 9);
}

void NetUtils::decodeSplitFromBuffer(const std::vector<unsigned char>& buffer, Split& split) {
    std::vector<unsigned char> idBuffer(buffer.begin(), buffer.begin() + 4);
    decodeIntFromUchar(idBuffer, split.id);
    
    std::vector<unsigned char> lengthBuffer(buffer.begin() + 4, buffer.begin() + 8);
    int contentLengthInt;
    decodeIntFromUchar(lengthBuffer, contentLengthInt);
    split.content_length = static_cast<size_t>(contentLengthInt);
    
    split.content.resize(split.content_length);
    std::copy(buffer.begin() + 8, buffer.begin() + 8 + split.content_length, split.content.begin());
}

// 私有函数实现
void NetUtils::encodeIntToUchar(unsigned char* buffer, int n) {
    buffer[0] = (n >> 24) & 0xFF;
    buffer[1] = (n >> 16) & 0xFF;
    buffer[2] = (n >> 8) & 0xFF;
    buffer[3] = n & 0xFF;
}

void NetUtils::decodeIntFromUchar(const unsigned char* buffer, int& n) {
    int temp = 0;
    temp = buffer[0] << 24;
    temp |= buffer[1] << 16;
    temp |= buffer[2] << 8;
    temp |= buffer[3];
    n = temp;
}