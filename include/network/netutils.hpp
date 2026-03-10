#ifndef NETUTILS_HPP
#define NETUTILS_HPP

#include "utils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <string>

constexpr int MAX_SEG_SIZE = 100 * 1024 * 1024;
constexpr int INT_SIZE = 4;
constexpr unsigned char INITIAL_WRITE_FLAG = 0;
constexpr unsigned char CHUNK_WRITE_FLAG = 1;
constexpr unsigned char FINAL_WRITE_FLAG = 2;
constexpr char RESET_SIG = 'N';
constexpr char PROCEED_SIG = 'Y';
constexpr char END_GET_SIG = 'E';
constexpr int CHUNK_INFO_STRUCT_SIZE = MAX_CHAR_BUFF + NUM_SERVER * INT_SIZE;

constexpr const char* GENERIC_TEMPLATE = "FLAG %d %[^\n]s";
constexpr const char* AUTH_TEMPLATE = "FLAG %d USERNAME %s PASSWORD %s";
constexpr const char* GET_TEMPLATE = "FLAG %d USERNAME %s PASSWORD %s FOLDER %s FILENAME %s\n";
constexpr const char* PUT_TEMPLATE = "FLAG %d USERNAME %s PASSWORD %s FOLDER %s FILENAME %s\n";
constexpr const char* LIST_TEMPLATE = "FLAG %d USERNAME %s PASSWORD %s FOLDER %s FILENAME %s\n";
constexpr const char* MKDIR_TEMPLATE = "FLAG %d USERNAME %s PASSWORD %s FOLDER %s FILENAME %s\n";
constexpr const char* AUTH_OK = "AUTH_OK";
constexpr const char* AUTH_NOT_OK = "AUTH_NOT_OK";

enum CommandFlag {
    LIST_FLAG = 0,
    GET_FLAG = 1,
    PUT_FLAG = 2,
    MKDIR_FLAG = 3,
    AUTH_FLAG = 4
};

class NetUtils {
public:
    static void fetchAndPrintError(int socket);
    
    static void sendIntValueSocket(int socket, int value);
    static void recvIntValueSocket(int socket, int& value);
    static void encodeIntToUchar(std::vector<unsigned char>& buffer, int n);
    static void decodeIntFromUchar(const std::vector<unsigned char>& buffer, int& n);
    
    static int encodeUserStruct(std::string& buffer, const User& user);
    static void decodeUserStruct(const std::string& buffer, User& user);
    
    static int sendToSocket(int socket, const std::vector<unsigned char>& payload);
    static int recvFromSocket(int socket, std::vector<unsigned char>& payload);
    static void sendSignal(const std::vector<int>& connFds, unsigned char signal);
    static void recvSignal(int socket, unsigned char& payload);
    
    static void encodeServerChunksInfoToBuffer(std::vector<unsigned char>& buffer, 
                                              const ServerChunksInfo& serverChunksInfo);
    static void decodeServerChunksInfoFromBuffer(const std::vector<unsigned char>& buffer, 
                                                ServerChunksInfo& serverChunksInfo);
    static void encodeChunkInfoToBuffer(std::vector<unsigned char>& buffer, 
                                       const ChunkInfo& chunkInfo);
    static void decodeChunkInfoFromBuffer(const std::vector<unsigned char>& buffer, 
                                         ChunkInfo& chunkInfo);
    
    static void writeSplitToSocketAsStream(int socket, const Split& split);
    static void writeSplitFromSocketAsStream(int socket, Split& split);
    
    static void writeMultipleSplitsToSocket(int socket, const std::vector<Split*>& splits);
    
    static void encodeSplitToBuffer(std::vector<unsigned char>& buffer, const Split& split);
    static void decodeSplitFromBuffer(const std::vector<unsigned char>& buffer, Split& split);

private:
    static void encodeIntToUchar(unsigned char* buffer, int n);
    static void decodeIntFromUchar(const unsigned char* buffer, int& n);
};

#endif