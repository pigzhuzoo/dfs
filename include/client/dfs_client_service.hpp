#ifndef DFS_CLIENT_SERVICE_HPP
#define DFS_CLIENT_SERVICE_HPP

#include "dfcutils.hpp"
#include "thread_pool.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <queue>
#include <condition_variable>

namespace dfs {

struct UserInfo {
    std::string username;
    std::string password;
    std::string home_folder;
    size_t quota_bytes;
    size_t used_bytes;
};

struct FileOperation {
    enum Type { PUT, GET, LIST, MKDIR, DELETE };
    Type type;
    std::string local_path;
    std::string remote_path;
    std::string folder;
    std::function<void(bool, const std::string&)> callback;
};

struct OperationResult {
    bool success;
    std::string message;
    double latency_ms;
    double throughput_mbps;
};

class DfsClientSession {
public:
    DfsClientSession(const UserInfo& user, const DfcConfig& config);
    ~DfsClientSession();
    
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    OperationResult putFile(const std::string& localPath, const std::string& remoteName);
    OperationResult getFile(const std::string& remoteName, const std::string& localPath);
    OperationResult listFiles(const std::string& folder = "/");
    OperationResult mkdir(const std::string& folder);
    
    const UserInfo& getUserInfo() const { return user_; }
    size_t getBytesTransferred() const { return bytesTransferred_; }
    
private:
    UserInfo user_;
    DfcConfig config_;
    std::vector<int> connFds_;
    bool connected_;
    mutable std::mutex mutex_;
    size_t bytesTransferred_;
    
    std::string getRemotePath(const std::string& path);
};

class DfsClientService {
public:
    static DfsClientService& getInstance();
    
    bool initialize(const std::string& configPath);
    void shutdown();
    
    std::string createSession(const std::string& username, const std::string& password);
    bool closeSession(const std::string& sessionId);
    std::shared_ptr<DfsClientSession> getSession(const std::string& sessionId);
    
    std::future<OperationResult> asyncPutFile(const std::string& sessionId,
                                              const std::string& localPath,
                                              const std::string& remoteName);
    std::future<OperationResult> asyncGetFile(const std::string& sessionId,
                                              const std::string& remoteName,
                                              const std::string& localPath);
    
    void submitOperation(const std::string& sessionId, FileOperation op);
    
    size_t getActiveSessionCount() const;
    std::vector<std::string> getActiveSessions() const;
    
    struct Stats {
        size_t total_sessions;
        size_t active_operations;
        size_t total_bytes_uploaded;
        size_t total_bytes_downloaded;
        double avg_throughput_mbps;
    };
    Stats getStats() const;
    
private:
    DfsClientService();
    ~DfsClientService();
    DfsClientService(const DfsClientService&) = delete;
    DfsClientService& operator=(const DfsClientService&) = delete;
    
    std::map<std::string, std::shared_ptr<DfsClientSession>> sessions_;
    mutable std::mutex sessionsMutex_;
    std::string configPath_;
    bool initialized_;
    
    std::string generateSessionId();
};

class MultiTenantTester {
public:
    struct TenantConfig {
        std::string username;
        std::string password;
        int num_connections;
        int num_files;
        size_t file_size;
    };
    
    struct TenantResult {
        std::string username;
        int successful_ops;
        int failed_ops;
        double total_time_sec;
        double avg_throughput_mbps;
        double p50_latency_ms;
        double p95_latency_ms;
        double p99_latency_ms;
    };
    
    struct TestConfig {
        std::vector<TenantConfig> tenants;
        int iterations;
        bool warmup;
        int warmup_iterations;
        std::string output_dir;
    };
    
    struct TestResult {
        std::vector<TenantResult> tenant_results;
        double total_throughput_mbps;
        double aggregate_throughput_mbps;
        size_t total_bytes;
        double total_time_sec;
        std::map<std::string, double> per_tenant_throughput;
    };
    
    MultiTenantTester();
    ~MultiTenantTester();
    
    bool initialize(const std::string& configPath);
    TestResult runTest(const TestConfig& config);
    void generateReport(const TestResult& result, const std::string& outputPath);
    
private:
    std::string configPath_;
    ThreadPool pool_;
    
    TenantResult runTenantTest(const TenantConfig& config, int iterations);
    std::vector<double> measureLatencies(const std::string& sessionId,
                                         const TenantConfig& config,
                                         int iterations);
};

}

#endif
