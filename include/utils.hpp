#ifndef UTILS_HPP
#define UTILS_HPP

#include "debug.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <openssl/md5.h>
#include <glob.h>

namespace fs = std::filesystem;

// 常量定义
constexpr int CHUNKS_PER_SERVER = 2;
constexpr int MAX_SERVERS = 10;
constexpr int NUM_SERVER = 4;
constexpr int MAX_CHAR_BUFF = 100;
constexpr int MAX_FILE_BUFF = 100;
constexpr int MAX_NUM_FILES = 100;

// 用户结构体
struct User {
    std::string username;
    std::string password;
    
    bool operator==(const User& other) const {
        return username == other.username && password == other.password;
    }
};

// 文件分割结构体
struct Split {
    int id;
    std::vector<unsigned char> content;
    size_t content_length;
    
    Split() : id(0), content_length(0) {}
    Split(int id_, const std::vector<unsigned char>& content_) 
        : id(id_), content(content_), content_length(content_.size()) {}
};

// 文件分割集合结构体
struct FileSplit {
    std::string file_name;
    std::array<std::unique_ptr<Split>, NUM_SERVER> splits;
    int split_count;
    
    FileSplit() : split_count(0) {}
};

// 文件属性结构体
struct FileAttribute {
    std::string remote_file_name;
    std::string remote_file_folder;
    std::string local_file_name;
    std::string local_file_folder;
};

// 块信息结构体
struct ChunkInfo {
    std::string file_name;
    std::array<int, CHUNKS_PER_SERVER> chunks;
    
    ChunkInfo() : chunks{0, 0} {}
};

// 服务器块信息结构体
struct ServerChunksInfo {
    int chunks;
    std::vector<ChunkInfo> chunk_info;
    
    ServerChunksInfo() : chunks(0) {}
};

// 服务器块聚合结构体
struct ServerChunksCollate {
    std::array<std::array<char, MAX_CHAR_BUFF>, MAX_NUM_FILES> file_names;
    std::array<std::array<bool, NUM_SERVER>, MAX_NUM_FILES> chunks;
    int num_files;
    
    ServerChunksCollate() : num_files(0) {
        // 初始化数组
        for (auto& file_row : file_names) {
            file_row.fill('\0');
        }
        for (auto& chunk_row : chunks) {
            chunk_row.fill(false);
        }
    }
};

class Utils {
public:
    // 文件和目录检查
    static bool checkFileExists(const std::string& directory, const std::string& fileName);
    static bool checkDirectoryExists(const std::string& path);
    
    // 字符串处理
    static std::string getToken(const std::string& str, const std::string& delim, int offset);
    static std::string getSubstringAfter(const std::string& haystack, const std::string& needle);
    static bool compareString(const std::string& str1, const std::string& str2);
    static int getCountChar(const std::string& buffer, char chr);
    static std::string getFileNameFromPath(const std::string& buffer);
    
    // 文件操作
    static bool getFilesInFolder(const std::string& folder, ServerChunksInfo& serverChunks, 
                                const std::string& checkFileName = "");
    static int getFoldersInFolder(const std::string& folderPath, std::vector<unsigned char>& payload);
    static void readIntoSplitFromFile(const std::string& filePath, Split& split);
    static void writeSplitToFile(const Split& split, const std::string& fileFolder, 
                                const std::string& fileName);
    
    // 加密解密
    static void encryptDecryptFileSplit(FileSplit& fileSplit, const std::string& key);
    
    // 哈希计算
    static int getMd5SumHashMod(const std::string& filePath);
    static void printHashValue(const std::vector<unsigned char>& buffer);
    
    // 内存管理
    static void freeFileSplit(FileSplit& fileSplit);
    static void freeSplit(Split& split);
    
    // 调试打印
    static void printFileSplit(const FileSplit& fileSplit);
    static void printSplit(const Split& split);
    static void printServerChunksCollate(const ServerChunksCollate& serverChunksCollate);
    static void printServerChunksInfo(const ServerChunksInfo& serverChunks);
    static void printChunkInfo(const ChunkInfo& chunkInfo);
    
    // 辅助函数
    static void extractFileNameAndFolder(const std::string& buffer, FileAttribute& fileAttr, int flag);
    static int checkFileNameExist(const std::array<std::array<char, MAX_CHAR_BUFF>, MAX_NUM_FILES>& fileNames, 
                                 const std::string& fileName, int n);
    static void insertToServerChunksCollate(ServerChunksCollate& serverChunksCollate, 
                                          const ServerChunksInfo& serverChunksInfo);
    static bool checkComplete(const std::array<bool, NUM_SERVER>& flagArray);

private:
    static constexpr char ROOT_FOLDER_STR = '/';
    static constexpr int EXTRACT_LOCAL = 0;
    static constexpr int EXTRACT_REMOTE = 1;
};

#endif // UTILS_HPP