#ifndef CONNECTION_POOL_HPP
#define CONNECTION_POOL_HPP

#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>

struct DfcServer {
    std::string name;
    std::string address;
    int port;
};

class ConnectionPool {
public:
    static ConnectionPool& getInstance() {
        static ConnectionPool instance;
        return instance;
    }

    int getConnection(const DfcServer& server) {
        std::string key = server.address + ":" + std::to_string(server.port);
        
        std::lock_guard<std::mutex> lock(poolMutex_);
        
        auto it = connections_.find(key);
        if (it != connections_.end() && it->second.fd != -1) {
            if (isConnectionValid(it->second.fd)) {
                it->second.lastUsed = std::chrono::steady_clock::now();
                return it->second.fd;
            } else {
                close(it->second.fd);
                connections_.erase(it);
            }
        }
        
        int fd = createConnection(server);
        if (fd != -1) {
            ConnectionInfo info;
            info.fd = fd;
            info.lastUsed = std::chrono::steady_clock::now();
            connections_[key] = info;
        }
        return fd;
    }

    void releaseConnection(const std::string& key) {
        std::lock_guard<std::mutex> lock(poolMutex_);
        auto it = connections_.find(key);
        if (it != connections_.end()) {
            it->second.lastUsed = std::chrono::steady_clock::now();
        }
    }

    void closeAll() {
        std::lock_guard<std::mutex> lock(poolMutex_);
        for (auto& pair : connections_) {
            if (pair.second.fd != -1) {
                close(pair.second.fd);
            }
        }
        connections_.clear();
    }

    void cleanupIdle(std::chrono::seconds maxIdleTime = std::chrono::seconds(30)) {
        std::lock_guard<std::mutex> lock(poolMutex_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = connections_.begin(); it != connections_.end(); ) {
            auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.lastUsed);
            if (idleTime > maxIdleTime) {
                close(it->second.fd);
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    ConnectionPool() = default;
    ~ConnectionPool() { closeAll(); }
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    struct ConnectionInfo {
        int fd = -1;
        std::chrono::steady_clock::time_point lastUsed;
    };

    int createConnection(const DfcServer& server);
    bool isConnectionValid(int fd);

    std::unordered_map<std::string, ConnectionInfo> connections_;
    std::mutex poolMutex_;
};

#endif
