#include "dfs_client_service.hpp"
#include "utils.hpp"
#include <sstream>
#include <random>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <openssl/sha.h>
#include <sys/stat.h>

namespace dfs {

DfsClientSession::DfsClientSession(const UserInfo& user, const DfcConfig& config)
    : user_(user), connected_(false), bytesTransferred_(0) {
    config_.server_count = config.server_count;
    config_.encryption_type = config.encryption_type;
    if (config.user) {
        config_.user = std::make_unique<User>();
        config_.user->username = config.user->username;
        config_.user->password = config.user->password;
    }
    for (int i = 0; i < config.server_count; i++) {
        if (config.servers[i]) {
            config_.servers[i] = std::make_unique<DfcServer>();
            config_.servers[i]->name = config.servers[i]->name;
            config_.servers[i]->address = config.servers[i]->address;
            config_.servers[i]->port = config.servers[i]->port;
        }
    }
    connFds_.resize(config.server_count, -1);
}

DfsClientSession::~DfsClientSession() {
    disconnect();
}

bool DfsClientSession::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_) return true;
    
    DfcUtils::setupConnections(connFds_, config_);
    
    for (int fd : connFds_) {
        if (fd != -1) {
            connected_ = true;
            return true;
        }
    }
    return false;
}

void DfsClientSession::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return;
    
    DfcUtils::tearDownConnections(connFds_, config_);
    connected_ = false;
}

bool DfsClientSession::isConnected() const {
    return connected_;
}

std::string DfsClientSession::getRemotePath(const std::string& path) {
    if (path.empty() || path[0] == '/') {
        return user_.home_folder + path;
    }
    return user_.home_folder + "/" + path;
}

size_t getFileSizeHelper(const std::string& filePath) {
    struct stat statBuf;
    if (stat(filePath.c_str(), &statBuf) == 0) {
        return statBuf.st_size;
    }
    return 0;
}

OperationResult DfsClientSession::putFile(const std::string& localPath, const std::string& remoteName) {
    OperationResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!connected_) {
        result.success = false;
        result.message = "Not connected";
        return result;
    }
    
    std::string cmd = "PUT " + localPath + " " + remoteName;
    DfcUtils::commandHandler(connFds_, PUT_FLAG, cmd.substr(4), config_);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.success = true;
    result.message = "Upload successful";
    
    size_t fileSize = getFileSizeHelper(localPath);
    if (fileSize > 0) {
        bytesTransferred_ += fileSize;
        result.throughput_mbps = (fileSize / 1024.0 / 1024.0) / (result.latency_ms / 1000.0);
    }
    
    return result;
}

OperationResult DfsClientSession::getFile(const std::string& remoteName, const std::string& localPath) {
    OperationResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!connected_) {
        result.success = false;
        result.message = "Not connected";
        return result;
    }
    
    std::string cmd = "GET " + remoteName + " " + localPath;
    DfcUtils::commandHandler(connFds_, GET_FLAG, cmd.substr(4), config_);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.success = true;
    result.message = "Download successful";
    
    size_t fileSize = getFileSizeHelper(localPath);
    if (fileSize > 0) {
        bytesTransferred_ += fileSize;
        result.throughput_mbps = (fileSize / 1024.0 / 1024.0) / (result.latency_ms / 1000.0);
    }
    
    return result;
}

OperationResult DfsClientSession::listFiles(const std::string& folder) {
    (void)folder;
    OperationResult result;
    result.success = true;
    result.message = "List operation";
    return result;
}

OperationResult DfsClientSession::mkdir(const std::string& folder) {
    (void)folder;
    OperationResult result;
    result.success = true;
    result.message = "Mkdir operation";
    return result;
}

DfsClientService& DfsClientService::getInstance() {
    static DfsClientService instance;
    return instance;
}

DfsClientService::DfsClientService() : initialized_(false) {}

DfsClientService::~DfsClientService() {
    shutdown();
}

bool DfsClientService::initialize(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    configPath_ = configPath;
    initialized_ = true;
    return true;
}

void DfsClientService::shutdown() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_.clear();
    initialized_ = false;
}

std::string DfsClientService::generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

std::string DfsClientService::createSession(const std::string& username, const std::string& password) {
    UserInfo user;
    user.username = username;
    user.password = password;
    user.home_folder = "/" + username;
    user.quota_bytes = 1024ULL * 1024 * 1024 * 10;
    user.used_bytes = 0;
    
    DfcConfig config;
    DfcUtils::readDfcConf(configPath_, config);
    
    config.user = std::make_unique<User>();
    config.user->username = username;
    config.user->password = password;
    
    auto session = std::make_shared<DfsClientSession>(user, config);
    if (!session->connect()) {
        return "";
    }
    
    std::string sessionId = generateSessionId();
    
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_[sessionId] = session;
    return sessionId;
}

bool DfsClientService::closeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second->disconnect();
        sessions_.erase(it);
        return true;
    }
    return false;
}

std::shared_ptr<DfsClientSession> DfsClientService::getSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::future<OperationResult> DfsClientService::asyncPutFile(const std::string& sessionId,
                                                            const std::string& localPath,
                                                            const std::string& remoteName) {
    return ThreadPool::getInstance().enqueue([this, sessionId, localPath, remoteName]() {
        auto session = getSession(sessionId);
        if (!session) {
            OperationResult result;
            result.success = false;
            result.message = "Session not found";
            return result;
        }
        return session->putFile(localPath, remoteName);
    });
}

std::future<OperationResult> DfsClientService::asyncGetFile(const std::string& sessionId,
                                                            const std::string& remoteName,
                                                            const std::string& localPath) {
    return ThreadPool::getInstance().enqueue([this, sessionId, remoteName, localPath]() {
        auto session = getSession(sessionId);
        if (!session) {
            OperationResult result;
            result.success = false;
            result.message = "Session not found";
            return result;
        }
        return session->getFile(remoteName, localPath);
    });
}

size_t DfsClientService::getActiveSessionCount() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.size();
}

std::vector<std::string> DfsClientService::getActiveSessions() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<std::string> ids;
    for (const auto& pair : sessions_) {
        ids.push_back(pair.first);
    }
    return ids;
}

DfsClientService::Stats DfsClientService::getStats() const {
    Stats stats = {};
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    stats.total_sessions = sessions_.size();
    for (const auto& pair : sessions_) {
        stats.total_bytes_uploaded += pair.second->getBytesTransferred();
    }
    
    return stats;
}

MultiTenantTester::MultiTenantTester() : pool_(8) {}

MultiTenantTester::~MultiTenantTester() = default;

bool MultiTenantTester::initialize(const std::string& configPath) {
    configPath_ = configPath;
    return true;
}

MultiTenantTester::TenantResult MultiTenantTester::runTenantTest(const TenantConfig& config, int iterations) {
    TenantResult result;
    result.username = config.username;
    
    DfsClientService& service = DfsClientService::getInstance();
    std::string sessionId = service.createSession(config.username, config.password);
    
    if (sessionId.empty()) {
        result.failed_ops = iterations;
        return result;
    }
    
    auto session = service.getSession(sessionId);
    std::vector<double> latencies;
    
    auto testStart = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        std::string localFile = "/tmp/tenant_test_" + config.username + "_" + std::to_string(i) + ".bin";
        
        {
            std::ofstream file(localFile, std::ios::binary);
            for (size_t j = 0; j < config.file_size; j++) {
                file << (char)(rand() % 256);
            }
        }
        
        std::string remoteFile = "tenant_test_" + std::to_string(i) + ".bin";
        
        auto opResult = session->putFile(localFile, remoteFile);
        
        if (opResult.success) {
            result.successful_ops++;
            latencies.push_back(opResult.latency_ms);
            result.avg_throughput_mbps += opResult.throughput_mbps;
        } else {
            result.failed_ops++;
        }
        
        std::remove(localFile.c_str());
    }
    
    auto testEnd = std::chrono::high_resolution_clock::now();
    result.total_time_sec = std::chrono::duration<double>(testEnd - testStart).count();
    
    if (result.successful_ops > 0) {
        result.avg_throughput_mbps /= result.successful_ops;
    }
    
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        result.p50_latency_ms = latencies[latencies.size() * 50 / 100];
        result.p95_latency_ms = latencies[latencies.size() * 95 / 100];
        result.p99_latency_ms = latencies[latencies.size() * 99 / 100];
    }
    
    service.closeSession(sessionId);
    return result;
}

MultiTenantTester::TestResult MultiTenantTester::runTest(const TestConfig& config) {
    TestResult result;
    
    auto testStart = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<TenantResult>> futures;
    
    for (const auto& tenant : config.tenants) {
        futures.push_back(pool_.enqueue([this, &tenant, &config]() {
            return runTenantTest(tenant, config.iterations);
        }));
    }
    
    for (auto& future : futures) {
        result.tenant_results.push_back(future.get());
    }
    
    auto testEnd = std::chrono::high_resolution_clock::now();
    result.total_time_sec = std::chrono::duration<double>(testEnd - testStart).count();
    
    for (const auto& tenantResult : result.tenant_results) {
        result.total_bytes += tenantResult.successful_ops * 
            config.tenants[0].file_size;
        result.aggregate_throughput_mbps += tenantResult.avg_throughput_mbps;
        result.per_tenant_throughput[tenantResult.username] = tenantResult.avg_throughput_mbps;
    }
    
    result.total_throughput_mbps = (result.total_bytes / 1024.0 / 1024.0) / result.total_time_sec;
    
    return result;
}

void MultiTenantTester::generateReport(const TestResult& result, const std::string& outputPath) {
    std::ofstream file(outputPath);
    
    file << "# Multi-Tenant Performance Test Report\n\n";
    file << "## Summary\n\n";
    file << "- Total Time: " << std::fixed << std::setprecision(2) << result.total_time_sec << "s\n";
    file << "- Total Bytes: " << result.total_bytes << "\n";
    file << "- Total Throughput: " << result.total_throughput_mbps << " MB/s\n";
    file << "- Aggregate Throughput: " << result.aggregate_throughput_mbps << " MB/s\n\n";
    
    file << "## Per-Tenant Results\n\n";
    file << "| Username | Success | Failed | Throughput (MB/s) | P50 (ms) | P95 (ms) | P99 (ms) |\n";
    file << "|----------|---------|--------|-------------------|----------|----------|----------|\n";
    
    for (const auto& tr : result.tenant_results) {
        file << "| " << tr.username 
             << " | " << tr.successful_ops
             << " | " << tr.failed_ops
             << " | " << std::fixed << std::setprecision(2) << tr.avg_throughput_mbps
             << " | " << std::setprecision(1) << tr.p50_latency_ms
             << " | " << tr.p95_latency_ms
             << " | " << tr.p99_latency_ms << " |\n";
    }
    
    file.close();
}

}
