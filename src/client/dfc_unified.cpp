#include "dfcutils.hpp"
#include "dfs_client_service.hpp"
#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include <sstream>

void printHelp() {
    std::cout << "DFS Client - Unified Mode\n"
              << "Usage:\n"
              << "  dfc-unified <config_file> [--service] [--help]\n\n"
              << "Options:\n"
              << "  --service   Run in service mode (multi-tenant API)\n"
              << "  --help      Show this help message\n\n"
              << "Interactive commands:\n"
              << "  LIST [folder]        List files in folder\n"
              << "  PUT <local> <remote> Upload file\n"
              << "  GET <remote> <local> Download file\n"
              << "  MKDIR <folder>       Create folder\n"
              << "  EXIT/QUIT           Exit client\n\n"
              << "Service mode commands (if --service):\n"
              << "  (See multi-tenant API documentation)\n";
}

int runInteractiveMode(const std::string& confFile) {
    std::cout << "<<< DFS Client - Interactive Mode >>>\n";
    
    DfcConfig conf;
    std::string buffer;
    std::vector<int> connFds;
    FileSplit fileSplit;
    
    DfcUtils::readDfcConf(confFile, conf);
    
    while (true) {
        buffer.clear();
        memset(&fileSplit, 0, sizeof(fileSplit));
        
        std::cout << ">>> " << std::flush;
        std::getline(std::cin, buffer);
        
        if (buffer == "EXIT" || buffer == "QUIT" || buffer == "exit" || buffer == "quit") {
            std::cout << "<<< Goodbye!" << std::endl;
            break;
        }
        
        if (buffer == "HELP" || buffer == "help") {
            printHelp();
            continue;
        }
        
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
            std::cout << "<<< Invalid command. Type HELP for available commands." << std::endl;
        }
        
        DfcUtils::tearDownConnections(connFds, conf);
    }
    
    DfcUtils::freeDfcConf(conf);
    return 0;
}

int runServiceMode(const std::string& confFile) {
    std::cout << "<<< DFS Client - Service Mode >>>\n";
    std::cout << "Multi-tenant service is running...\n";
    std::cout << "Type HELP for available commands, EXIT to quit.\n";
    
    dfs::DfsClientService& service = dfs::DfsClientService::getInstance();
    
    if (!service.initialize(confFile)) {
        std::cerr << "Failed to initialize service!" << std::endl;
        return 1;
    }
    
    std::string buffer;
    std::map<std::string, std::string> userSessions;
    
    while (true) {
        std::cout << "service>>> " << std::flush;
        std::getline(std::cin, buffer);
        
        if (buffer == "EXIT" || buffer == "QUIT" || buffer == "exit" || buffer == "quit") {
            std::cout << "<<< Shutting down service..." << std::endl;
            break;
        }
        
        if (buffer == "HELP" || buffer == "help") {
            std::cout << "Service mode commands:\n"
                      << "  login <user> <pass>     Create user session\n"
                      << "  logout <session>      Close user session\n"
                      << "  sessions              List active sessions\n"
                      << "  stats                 Show service stats\n"
                      << "  put <session> <local> <remote>  Upload file\n"
                      << "  get <session> <remote> <local>  Download file\n"
                      << "  HELP                  Show this help\n"
                      << "  EXIT/QUIT             Exit service\n";
            continue;
        }
        
        if (buffer.substr(0, 6) == "login ") {
            std::string args = buffer.substr(6);
            size_t space = args.find(' ');
            if (space != std::string::npos) {
                std::string user = args.substr(0, space);
                std::string pass = args.substr(space + 1);
                
                std::string sessionId = service.createSession(user, pass);
                if (!sessionId.empty()) {
                    userSessions[user] = sessionId;
                    std::cout << "<<< Session created: " << sessionId << std::endl;
                } else {
                    std::cout << "<<< Failed to create session!" << std::endl;
                }
            } else {
                std::cout << "<<< Usage: login <user> <password>" << std::endl;
            }
            continue;
        }
        
        if (buffer.substr(0, 7) == "logout ") {
            std::string sessionId = buffer.substr(7);
            if (service.closeSession(sessionId)) {
                std::cout << "<<< Session closed" << std::endl;
                for (auto it = userSessions.begin(); it != userSessions.end(); ) {
                    if (it->second == sessionId) {
                        it = userSessions.erase(it);
                    } else {
                        ++it;
                    }
                }
            } else {
                std::cout << "<<< Session not found!" << std::endl;
            }
            continue;
        }
        
        if (buffer == "sessions" || buffer == "SESSIONS") {
            std::vector<std::string> sessions = service.getActiveSessions();
            std::cout << "<<< Active sessions (" << sessions.size() << "):\n";
            for (const auto& s : sessions) {
                std::cout << "  " << s << std::endl;
            }
            continue;
        }
        
        if (buffer == "stats" || buffer == "STATS") {
            auto stats = service.getStats();
            std::cout << "<<< Service stats:\n"
                      << "  Total sessions: " << stats.total_sessions << "\n"
                      << "  Active operations: " << stats.active_operations << "\n"
                      << "  Total bytes uploaded: " << stats.total_bytes_uploaded << "\n"
                      << "  Total bytes downloaded: " << stats.total_bytes_downloaded << "\n"
                      << "  Avg throughput: " << stats.avg_throughput_mbps << " MB/s\n";
            continue;
        }
        
        if (buffer.substr(0, 4) == "put ") {
            std::string args = buffer.substr(4);
            std::istringstream iss(args);
            std::vector<std::string> tokens;
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            
            if (tokens.size() >= 3) {
                std::string sessionId = tokens[0];
                std::string localPath = tokens[1];
                std::string remoteName = tokens[2];
                
                auto result = service.asyncPutFile(sessionId, localPath, remoteName).get();
                if (result.success) {
                    std::cout << "<<< Upload successful! Latency: " << result.latency_ms << " ms, Throughput: " << result.throughput_mbps << " MB/s\n";
                } else {
                    std::cout << "<<< Upload failed: " << result.message << "\n";
                }
            } else {
                std::cout << "<<< Usage: put <session> <local_path> <remote_name>\n";
            }
            continue;
        }
        
        if (buffer.substr(0, 4) == "get ") {
            std::string args = buffer.substr(4);
            std::istringstream iss(args);
            std::vector<std::string> tokens;
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            
            if (tokens.size() >= 3) {
                std::string sessionId = tokens[0];
                std::string remoteName = tokens[1];
                std::string localPath = tokens[2];
                
                auto result = service.asyncGetFile(sessionId, remoteName, localPath).get();
                if (result.success) {
                    std::cout << "<<< Download successful! Latency: " << result.latency_ms << " ms, Throughput: " << result.throughput_mbps << " MB/s\n";
                } else {
                    std::cout << "<<< Download failed: " << result.message << "\n";
                }
            } else {
                std::cout << "<<< Usage: get <session> <remote_name> <local_path>\n";
            }
            continue;
        }
        
        std::cout << "<<< Unknown command. Type HELP for available commands." << std::endl;
    }
    
    service.shutdown();
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--help") {
            printHelp();
            return 0;
        }
    }
    
    std::string confFile = argv[1];
    bool serviceMode = false;
    
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--service") {
            serviceMode = true;
        }
    }
    
    if (serviceMode) {
        return runServiceMode(confFile);
    } else {
        return runInteractiveMode(confFile);
    }
}
