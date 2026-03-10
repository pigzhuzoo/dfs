#include "utils.hpp"
#include "logger.hpp"
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
    
    serverChunks.chunks = validFiles;
    if (!checkFileName.empty()) {
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
        
        if (pair.second.size() >= 1) {
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
        split.content_length = 0;
        split.content.clear();
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

void Utils::encryptDecryptFileSplit(FileSplit& fileSplit, const std::string& key, 
                                 EncryptionType encryptionType, bool isEncrypt) {
    std::cerr << "[DEBUG] encryptDecryptFileSplit called, encryptionType=" << static_cast<int>(encryptionType) 
              << ", isEncrypt=" << isEncrypt << std::endl;
    dfs::crypto::EncryptionAlgorithm algo;
    switch (encryptionType) {
        case EncryptionType::AES_256_GCM:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_GCM;
            break;
        case EncryptionType::AES_256_ECB:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_ECB;
            break;
        case EncryptionType::AES_256_CBC:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_CBC;
            break;
        case EncryptionType::AES_256_CFB:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_CFB;
            break;
        case EncryptionType::AES_256_OFB:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_OFB;
            break;
        case EncryptionType::AES_256_CTR:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_CTR;
            break;
        case EncryptionType::SM4_ECB:
            algo = dfs::crypto::EncryptionAlgorithm::SM4_ECB;
            break;
        case EncryptionType::SM4_CBC:
            algo = dfs::crypto::EncryptionAlgorithm::SM4_CBC;
            break;
        case EncryptionType::SM4_CTR:
            algo = dfs::crypto::EncryptionAlgorithm::SM4_CTR;
            break;
        case EncryptionType::RSA_OAEP:
            algo = dfs::crypto::EncryptionAlgorithm::RSA_OAEP;
            break;
        case EncryptionType::AES_256_FPGA:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_FPGA;
            std::cerr << "[DEBUG] Using AES_256_FPGA algorithm" << std::endl;
            break;
        default:
            algo = dfs::crypto::EncryptionAlgorithm::AES_256_GCM;
            std::cerr << "[DEBUG] Using default AES_256_GCM algorithm" << std::endl;
            break;
    }
    
    std::cerr << "[DEBUG] Encryption algorithm mapped to: " << static_cast<int>(algo) << std::endl;
    std::vector<unsigned char> crypto_key = dfs::crypto::CryptoUtils::generateKeyFromPassword(key, algo);
    
    for (int i = 0; i < fileSplit.object_count && i < static_cast<int>(fileSplit.objects.size()); i++) {
        if (fileSplit.objects[i]) {
            std::vector<unsigned char> input_data = fileSplit.objects[i]->content;
            std::vector<unsigned char> output_data;
            
            bool success;
            if (isEncrypt) {
                success = dfs::crypto::CryptoUtils::encryptData(input_data, output_data, algo, crypto_key);
            } else {
                success = dfs::crypto::CryptoUtils::decryptData(input_data, output_data, algo, crypto_key);
            }
            
            if (success) {
                fileSplit.objects[i]->content = output_data;
                fileSplit.objects[i]->content_length = output_data.size();
            } else {
                std::cerr << "Encryption/decryption failed for object " << i << std::endl;
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
    fileSplit.file_size = 0;
    for (auto& obj : fileSplit.objects) {
        if (obj) {
            freeSplit(*obj);
            obj.reset();
        }
    }
    fileSplit.objects.clear();
    fileSplit.object_count = 0;
}

void Utils::freeSplit(Split& split) {
    split.content.clear();
    split.content_length = 0;
    split.id = 0;
    split.offset = 0;
}

void Utils::printFileSplit(const FileSplit& fileSplit) {
    DEBUGS("Printing File Split Struct (Ceph Style)");
    DEBUGSS("Filename", fileSplit.file_name.c_str());
    DEBUGSN("File size", static_cast<int>(fileSplit.file_size));
    DEBUGSN("Object size", static_cast<int>(fileSplit.object_size));
    DEBUGSN("Number of objects", fileSplit.object_count);
    for (int i = 0; i < fileSplit.object_count && i < static_cast<int>(fileSplit.objects.size()); i++) {
        if (fileSplit.objects[i]) {
            printSplit(*fileSplit.objects[i]);
            DEBUGS("");
        }
    }
}

void Utils::printSplit(const Split& split) {
    DEBUGSN("Object with id", split.id);
    DEBUGSN("Offset", static_cast<int>(split.offset));
    DEBUGSN("Content_length", static_cast<int>(split.content_length));
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
            serverChunksCollate.num_files++;
            j = serverChunksCollate.num_files - 1;
            strncpy(serverChunksCollate.file_names[j].data(), 
                   chunkInfo.file_name.c_str(), MAX_CHAR_BUFF - 1);
        }
        
        for (int k = 0; k < CHUNKS_PER_SERVER; k++) {
            int chunkNum = chunkInfo.chunks[k];
            if (chunkNum >= 0 && chunkNum < NUM_SERVER) {
                serverChunksCollate.chunks[j][chunkNum] = true;
            }
        }
    }
}

bool Utils::checkComplete(const std::array<bool, NUM_SERVER>& flagArray) {
    int sum = 0;
    for (int i = 0; i < NUM_SERVER; i++) {
        sum += flagArray[i] ? 1 : 0;
    }
    return sum >= 1;
}

int Utils::calculateObjectCount(size_t fileSize, size_t objectSize) {
    if (objectSize == 0) return 0;
    return static_cast<int>((fileSize + objectSize - 1) / objectSize);
}

size_t Utils::calculateOptimalObjectSize(size_t fileSize) {
    if (fileSize == 0) return DEFAULT_OBJECT_SIZE;
    
    if (fileSize <= MIN_OBJECT_SIZE) {
        return fileSize;
    }
    
    if (fileSize <= DEFAULT_OBJECT_SIZE) {
        return DEFAULT_OBJECT_SIZE;
    }
    
    int targetObjects = static_cast<int>((fileSize + DEFAULT_OBJECT_SIZE - 1) / DEFAULT_OBJECT_SIZE);
    if (targetObjects <= MAX_OBJECTS_PER_FILE) {
        return DEFAULT_OBJECT_SIZE;
    }
    
    size_t optimalSize = (fileSize + MAX_OBJECTS_PER_FILE - 1) / MAX_OBJECTS_PER_FILE;
    optimalSize = ((optimalSize + 1024 * 1024 - 1) / (1024 * 1024)) * (1024 * 1024);
    
    if (optimalSize > MAX_OBJECT_SIZE) {
        optimalSize = MAX_OBJECT_SIZE;
    }
    
    return optimalSize;
}

bool Utils::splitFileToObjects(const std::string& filePath, FileSplit& fileSplit, 
                               size_t objectSize) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filePath << std::endl;
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize == 0) {
        std::cerr << "File is empty: " << filePath << std::endl;
        file.close();
        return false;
    }
    
    if (objectSize == DEFAULT_OBJECT_SIZE) {
        objectSize = calculateOptimalObjectSize(fileSize);
    }
    
    if (objectSize < MIN_OBJECT_SIZE) {
        objectSize = MIN_OBJECT_SIZE;
    }
    if (objectSize > MAX_OBJECT_SIZE) {
        objectSize = MAX_OBJECT_SIZE;
    }
    
    fileSplit.file_name = filePath;
    fileSplit.file_size = fileSize;
    fileSplit.object_size = objectSize;
    
    int objectCount = calculateObjectCount(fileSize, objectSize);
    if (objectCount > MAX_OBJECTS_PER_FILE) {
        std::cerr << "Too many objects: " << objectCount << " (max: " << MAX_OBJECTS_PER_FILE << ")" << std::endl;
        file.close();
        return false;
    }
    
    fileSplit.objects.clear();
    fileSplit.objects.reserve(objectCount);
    fileSplit.object_count = objectCount;
    
    for (int i = 0; i < objectCount; i++) {
        size_t offset = static_cast<size_t>(i) * objectSize;
        size_t currentObjectSize = objectSize;
        
        if (offset + currentObjectSize > fileSize) {
            currentObjectSize = fileSize - offset;
        }
        
        auto obj = std::make_unique<Split>();
        obj->id = i;
        obj->offset = offset;
        obj->content_length = currentObjectSize;
        obj->content.resize(currentObjectSize);
        
        file.seekg(offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(obj->content.data()), currentObjectSize);
        
        fileSplit.objects.push_back(std::move(obj));
    }
    
    file.close();
    
    DEBUGSS("File split into objects", filePath.c_str());
    DEBUGSN("File size", static_cast<int>(fileSize));
    DEBUGSN("Object size", static_cast<int>(objectSize));
    DEBUGSN("Object count", objectCount);
    
    return true;
}

bool Utils::combineFileFromObjects(const std::string& outputPath, const FileSplit& fileSplit) {
    if (fileSplit.object_count == 0 || fileSplit.objects.empty()) {
        std::cerr << "No objects to combine" << std::endl;
        return false;
    }
    
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to create output file: " << outputPath << std::endl;
        return false;
    }
    
    std::vector<std::pair<size_t, const Split*>> sortedObjects;
    for (const auto& obj : fileSplit.objects) {
        if (obj) {
            sortedObjects.push_back({obj->offset, obj.get()});
        }
    }
    
    std::sort(sortedObjects.begin(), sortedObjects.end(),
              [](const std::pair<size_t, const Split*>& a, const std::pair<size_t, const Split*>& b) {
                  return a.first < b.first;
              });
    
    for (const auto& pair : sortedObjects) {
        const Split* obj = pair.second;
        file.seekp(obj->offset, std::ios::beg);
        file.write(reinterpret_cast<const char*>(obj->content.data()), obj->content_length);
    }
    
    file.close();
    
    DEBUGSS("Objects combined into file", outputPath.c_str());
    DEBUGSN("Total objects", static_cast<int>(sortedObjects.size()));
    
    return true;
}
