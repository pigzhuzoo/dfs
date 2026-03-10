#ifndef FPGA_AES_HPP
#define FPGA_AES_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>

#ifdef USE_FPGA
#include <CL/cl2.hpp>
#endif

namespace dfs {
namespace crypto {

struct FpgaAesStats {
    double kernel_time_ms;
    double h2d_time_ms;
    double d2h_time_ms;
    double total_time_ms;
    double throughput_gbps;
    bool success;
};

class FpgaAesContext {
public:
    static FpgaAesContext& getInstance();
    
    bool initialize(const std::string& xclbin_path);
    bool isInitialized() const { return initialized_; }
    void release();
    
    bool encrypt(const std::vector<unsigned char>& input,
                 std::vector<unsigned char>& output,
                 const std::vector<unsigned char>& key,
                 FpgaAesStats* stats = nullptr);
    
    bool decrypt(const std::vector<unsigned char>& input,
                 std::vector<unsigned char>& output,
                 const std::vector<unsigned char>& key,
                 FpgaAesStats* stats = nullptr);
    
    std::string getLastDeviceName() const;
    std::string getXclbinPath() const;
    
    static bool isFpgaAvailable();
    static std::vector<std::string> getAvailableDevices();
    
private:
    FpgaAesContext();
    ~FpgaAesContext();
    
    FpgaAesContext(const FpgaAesContext&) = delete;
    FpgaAesContext& operator=(const FpgaAesContext&) = delete;
    
    bool initialized_;
    std::string xclbin_path_;
    std::mutex mutex_;
    
#ifdef USE_FPGA
    cl::Context context_;
    cl::CommandQueue queue_;
    cl::Kernel kernel_encrypt_;
    cl::Kernel kernel_decrypt_;
    cl::Program program_;
    cl::Device device_;
    
    bool initOpenCL(const std::string& xclbin_path);
    cl_ulong getEventDuration(cl::Event& event);
#endif
};

class FpgaAes {
public:
    static bool encrypt(const std::vector<unsigned char>& input,
                       std::vector<unsigned char>& output,
                       const std::vector<unsigned char>& key,
                       const std::string& xclbin_path = "",
                       FpgaAesStats* stats = nullptr);
    
    static bool decrypt(const std::vector<unsigned char>& input,
                       std::vector<unsigned char>& output,
                       const std::vector<unsigned char>& key,
                       const std::string& xclbin_path = "",
                       FpgaAesStats* stats = nullptr);
    
    static bool isAvailable();
    static std::string getDefaultXclbinPath();
    
private:
    static std::vector<unsigned char> padData(const std::vector<unsigned char>& data);
    static std::vector<unsigned char> unpadData(const std::vector<unsigned char>& data);
};

} // namespace crypto
} // namespace dfs

#endif // FPGA_AES_HPP
