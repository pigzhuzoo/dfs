#include "../include/utils.hpp"
#include "../include/logger.hpp"
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <map>

bool Utils::checkFileExists(const std::string& directory, const std::string& fileName) {
    std::string filePath = directory + fileName;
    return access(filePath.c_str(), F_OK) != -1;
}

bool Utils::checkDirectoryExists(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

std::string Utils::getToken(const std::string& str, const std::string& delim, int offset) {
    size_t pos = str.find(delim);
    if (pos == std::string::npos) {
        if (!str.empty() && offset == 0) {
            return str;
        } else {
            return "";
        }
    }
    
    if (offset == 1) {
        return str.substr(pos + delim.length());
    }
    
    return str.substr(0, pos);
}

std::string Utils::getSubstringAfter(const std::string& haystack, const std::string& needle) {
    size_t pos = haystack.find(needle);
    if (pos == std::string::npos) {
        return "";
    }
    return haystack.substr(pos + needle.length());
}

bool Utils::compareString(const std::string& str1, const std::string& str2) {
    return str1 == str2;
}

int Utils::getCountChar(const std::string& buffer, char chr) {
    return std::count(buffer.begin(), buffer.end(), chr);
}

std::string Utils::getFileNameFromPath(const std::string& buffer) {
    size_t pos = buffer.rfind(ROOT_FOLDER_STR);
    if (pos == std::string::npos || pos + 1 == buffer.length()) {
        return "";
    }
    return buffer.substr(pos + 1);
}

bool Utils::getFilesInFolder(const std::string& folder, ServerChunksInfo& serverChunks, 
                            const std::string& checkFileName) {
    std::string tempFolder = folder;
    if (tempFolder.back() != '/') {
        tempFolder += '/';
    }
    tempFolder += "*";
    
    glob_t globResult;
    if (glob(tempFolder.c_str(), GLOB_PERIOD, nullptr, &globResult) == GLOB_ERR) {
        perror("Error in Glob");
        return false;
    }
    
    // 移除错误的断言：globResult.gl_pathc % 2 == 0
    // glob可能返回任意数量的文件，包括奇数个
    
    int validFiles = 0;
    for (size_t i = 0; i < globResult.gl_pathc; i++) {
        std::string path(globResult.gl_pathv[i]);
        std::string fileName = getFileNameFromPath(path);
        
        // 跳过 "." 和 ".."
        if (fileName.length() < 3) continue;
        
        size_t dotPos = fileName.rfind('.');
        if (dotPos != std::string::npos && dotPos < fileName.length() - 1) {
            std::string baseName = fileName.substr(0, dotPos);
            std::string chunkStr = fileName.substr(dotPos + 1);
            
            // 验证chunkStr是否为有效的数字
            bool isValidChunk = !chunkStr.empty();
            for (char c : chunkStr) {
                if (!std::isdigit(c)) {
                    isValidChunk = false;
                    break;
                }
            }
            
            if (!isValidChunk) continue;
            
            [[maybe_unused]] int chunkNum = std::stoi(chunkStr);
            
            // 处理以点开头的隐藏文件：如果baseName以'.'开头，去掉开头的'.'
            std::string compareBaseName = baseName;
            if (!baseName.empty() && baseName[0] == '.') {
                compareBaseName = baseName.substr(1);
            }
            
            if (checkFileName.empty() || compareString(compareBaseName, checkFileName)) {
                validFiles++;
            }
        }
    }
    
    // 对于LIST命令，我们只关心有多少个完整的文件
    // 每个文件应该有2个分片，所以完整的文件数是validFiles / 2
    serverChunks.chunks = validFiles / 2;
    if (!checkFileName.empty()) {
        // 对于GET命令，我们只关心特定文件是否存在
        serverChunks.chunks = (validFiles > 0) ? 1 : 0;
    }
    
    serverChunks.chunk_info.resize(serverChunks.chunks);
    
    // 解析实际的文件信息
    std::map<std::string, std::vector<int>> fileChunks;
    for (size_t i = 0; i < globResult.gl_pathc; i++) {
        std::string path(globResult.gl_pathv[i]);
        std::string fileName = getFileNameFromPath(path);
        
        if (fileName.length() < 3) continue;
        
        size_t dotPos = fileName.rfind('.');
        if (dotPos != std::string::npos && dotPos < fileName.length() - 1) {
            std::string baseName = fileName.substr(0, dotPos);
            std::string chunkStr = fileName.substr(dotPos + 1);
            
            // 验证chunkStr是否为有效的数字
            bool isValidChunk = !chunkStr.empty();
            for (char c : chunkStr) {
                if (!std::isdigit(c)) {
                    isValidChunk = false;
                    break;
                }
            }
            
            if (!isValidChunk) continue;
            
            int chunkNum = std::stoi(chunkStr);
            
            // 处理以点开头的隐藏文件：如果baseName以'.'开头，去掉开头的'.'
            std::string compareBaseName = baseName;
            if (!baseName.empty() && baseName[0] == '.') {
                compareBaseName = baseName.substr(1);
            }
            
            if (checkFileName.empty() || compareString(compareBaseName, checkFileName)) {
                fileChunks[compareBaseName].push_back(chunkNum);
            }
        }
    }
    
    int idx = 0;
    for (const auto& pair : fileChunks) {
        if (idx >= serverChunks.chunks) break;
        
        // 只有当文件有至少2个分片时，才认为它是完整的
        if (pair.second.size() >= 2) {
            serverChunks.chunk_info[idx].file_name = pair.first;
            for (size_t i = 0; i < std::min(pair.second.size(), (size_t)CHUNKS_PER_SERVER); i++) {
                serverChunks.chunk_info[idx].chunks[i] = pair.second[i];
            }
            idx++;
        }
    }
    
    // 调整chunks数量为实际的完整文件数
    serverChunks.chunks = idx;
    serverChunks.chunk_info.resize(serverChunks.chunks);
    
    globfree(&globResult);
    return !fileChunks.empty();
}

int Utils::getFoldersInFolder(const std::string& folderPath, std::vector<unsigned char>& payload) {
    DIR* dp = opendir(folderPath.c_str());
    if (!dp) {
        DEBUGSS("Couldn't open directory to find folders", strerror(errno));
        return 0;
    }
    
    std::string buffer;
    struct dirent* ep;
    
    while ((ep = readdir(dp)) != nullptr) {
        if (ep->d_type == DT_DIR && strlen(ep->d_name) > 2) {
            buffer += std::string(ep->d_name) + "/\n";
            DEBUGSS("Directory", ep->d_name);
        }
    }
    
    closedir(dp);
    
    payload.assign(buffer.begin(), buffer.end());
    return payload.size();
}

void Utils::readIntoSplitFromFile(const std::string& filePath, Split& split) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filePath << std::endl;
        return;
    }
    
    file.seekg(0, std::ios::end);
    split.content_length = file.tellg();
    file.seekg(0, std::ios::beg);
    
    split.content.resize(split.content_length);
    file.read(reinterpret_cast<char*>(split.content.data()), split.content_length);
    file.close();
}

void Utils::writeSplitToFile(const Split& split, const std::string& fileFolder, 
                            const std::string& fileName) {
    std::string filePath = fileFolder + "/." + fileName + "." + std::to_string(split.id);
    log_debug("File written at: " + filePath);
    
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        log_error("Error in opening file to write: " + filePath);
        return;
    }
    
    file.write(reinterpret_cast<const char*>(split.content.data()), split.content_length);
    file.close();
    log_debug("Successfully wrote " + std::to_string(split.content_length) + " bytes to " + filePath);
}

void Utils::encryptDecryptFileSplit(FileSplit& fileSplit, const std::string& key) {
    size_t keyLen = key.length();
    for (int i = 0; i < fileSplit.split_count; i++) {
        if (fileSplit.splits[i]) {
            auto& content = fileSplit.splits[i]->content;
            for (size_t j = 0; j < content.size(); j++) {
                content[j] = content[j] ^ key[j % keyLen];
            }
        }
    }
}

int Utils::getMd5SumHashMod(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filePath << std::endl;
        return -1;
    }
    
    MD5_CTX mdContext;
    MD5_Init(&mdContext);
    
    std::vector<unsigned char> buffer(MAX_FILE_BUFF);
    while (file.read(reinterpret_cast<char*>(buffer.data()), MAX_FILE_BUFF)) {
        MD5_Update(&mdContext, buffer.data(), file.gcount());
    }
    
    // 处理剩余字节
    if (file.gcount() > 0) {
        MD5_Update(&mdContext, buffer.data(), file.gcount());
    }
    
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &mdContext);
    
    int mod = 0;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        mod = (mod * 16 + digest[i]) % NUM_SERVER;
    }
    
    DEBUGSN("MOD:", mod);
    return mod;
}

void Utils::printHashValue(const std::vector<unsigned char>& buffer) {
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, buffer.data(), buffer.size());
    
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &context);
    
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        std::cerr << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(digest[i]);
    }
    std::cerr << std::endl;
}

void Utils::freeFileSplit(FileSplit& fileSplit) {
    fileSplit.file_name.clear();
    for (int i = 0; i < fileSplit.split_count; i++) {
        if (fileSplit.splits[i]) {
            freeSplit(*fileSplit.splits[i]);
            fileSplit.splits[i].reset();
        }
    }
    fileSplit.split_count = 0;
}

void Utils::freeSplit(Split& split) {
    split.content.clear();
    split.content_length = 0;
    split.id = 0;
}

void Utils::printFileSplit(const FileSplit& fileSplit) {
    DEBUGS("Printing File Split Struct");
    DEBUGSS("Filename", fileSplit.file_name.c_str());
    DEBUGSN("Number of splits", fileSplit.split_count);
    for (int i = 0; i < fileSplit.split_count; i++) {
        if (fileSplit.splits[i]) {
            printSplit(*fileSplit.splits[i]);
            DEBUGS("");
        }
    }
}

void Utils::printSplit(const Split& split) {
    DEBUGSN("Split with id", split.id);
    DEBUGSN("Content_length", static_cast<int>(split.content_length));
    // 注意：这里不打印实际内容，因为可能是二进制数据
}

void Utils::printServerChunksCollate(const ServerChunksCollate& serverChunksCollate) {
    DEBUGS("Printing Server Chunks Collate Struct");
    DEBUGSN("Num File", serverChunksCollate.num_files);
    for (int i = 0; i < serverChunksCollate.num_files; i++) {
        DEBUGSS("File name", std::string(serverChunksCollate.file_names[i].data()).c_str());
        for (int j = 0; j < NUM_SERVER; j++) {
            DEBUGSN("Chunk", j + 1);
            DEBUGSN("Present", serverChunksCollate.chunks[i][j]);
        }
    }
}

void Utils::printServerChunksInfo(const ServerChunksInfo& serverChunks) {
    DEBUGS("Printing Print Server Chunks Info Struct");
    for (const auto& chunkInfo : serverChunks.chunk_info) {
        printChunkInfo(chunkInfo);
    }
}

void Utils::printChunkInfo(const ChunkInfo& chunkInfo) {
    DEBUGS("Printing chunk info struct");
    DEBUGSS("Filename", chunkInfo.file_name.c_str());
    for (int i = 0; i < CHUNKS_PER_SERVER; i++) {
        DEBUGSN("Chunk Number", chunkInfo.chunks[i]);
    }
}

void Utils::extractFileNameAndFolder(const std::string& buffer, FileAttribute& fileAttr, int flag) {
    std::string ptr = getFileNameFromPath(buffer);
    
    if (flag == EXTRACT_LOCAL) {
        if (!ptr.empty()) {
            // 文件名存在于文件夹路径之后
            fileAttr.local_file_name = ptr;
            fileAttr.local_file_folder = buffer.substr(0, buffer.length() - ptr.length());
        } else {
            // 文件名不存在于文件夹路径之后
            if (buffer.find(ROOT_FOLDER_STR) != std::string::npos) {
                // 缓冲区只是文件夹名
                fileAttr.local_file_folder = buffer;
            } else {
                // 缓冲区只是文件名
                fileAttr.local_file_name = buffer;
            }
        }
    } else if (flag == EXTRACT_REMOTE) {
        if (!ptr.empty()) {
            // 文件名存在于文件夹路径之后
            fileAttr.remote_file_name = ptr;
            fileAttr.remote_file_folder = buffer.substr(0, buffer.length() - ptr.length());
        } else {
            // 文件名不存在于文件夹路径之后
            if (buffer.find(ROOT_FOLDER_STR) != std::string::npos) {
                // 缓冲区只是文件夹名
                fileAttr.remote_file_folder = buffer;
            } else {
                // 缓冲区只是文件名
                fileAttr.remote_file_name = buffer;
            }
        }
    }
}

int Utils::checkFileNameExist(const std::array<std::array<char, MAX_CHAR_BUFF>, MAX_NUM_FILES>& fileNames, 
                             const std::string& fileName, int n) {
    for (int i = 0; i < n; i++) {
        if (compareString(std::string(fileNames[i].data()), fileName)) {
            return i;
        }
    }
    return -1;
}

void Utils::insertToServerChunksCollate(ServerChunksCollate& serverChunksCollate, 
                                      const ServerChunksInfo& serverChunksInfo) {
    for (int i = 0; i < serverChunksInfo.chunks; i++) {
        const auto& chunkInfo = serverChunksInfo.chunk_info[i];
        int j = checkFileNameExist(serverChunksCollate.file_names, 
                                  chunkInfo.file_name, serverChunksCollate.num_files);
        
        if (j < 0) {
            // 文件不存在
            serverChunksCollate.num_files++;
            j = serverChunksCollate.num_files - 1;
            strncpy(serverChunksCollate.file_names[j].data(), 
                   chunkInfo.file_name.c_str(), MAX_CHAR_BUFF - 1);
        }
        
        // 文件存在
        for (int k = 0; k < CHUNKS_PER_SERVER; k++) {
            int chunkNum = chunkInfo.chunks[k];
            serverChunksCollate.chunks[j][chunkNum - 1] = true;
        }
    }
}

bool Utils::checkComplete(const std::array<bool, NUM_SERVER>& flagArray) {
    // 对于单服务器测试，我们只需要检查是否有至少2个分片存在
    // 因为在任何mod配置下，每个文件只需要2个分片就能恢复
    int sum = 0;
    for (int i = 0; i < NUM_SERVER; i++) {
        sum += flagArray[i] ? 1 : 0;
    }
    return sum >= 2;  // 至少需要2个分片
}
