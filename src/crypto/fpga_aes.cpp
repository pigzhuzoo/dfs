#include "crypto/fpga_aes.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdio>

#ifdef USE_FPGA
#include <CL/cl2.hpp>
#include <CL/cl_ext_xilinx.h>
#endif

namespace dfs {
namespace crypto {

#ifdef USE_FPGA

namespace {
std::vector<unsigned char> readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

void logError(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

void logInfo(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}
}

FpgaAesContext& FpgaAesContext::getInstance() {
    static FpgaAesContext instance;
    return instance;
}

FpgaAesContext::FpgaAesContext() : initialized_(false) {}

FpgaAesContext::~FpgaAesContext() {
    release();
}

bool FpgaAesContext::initOpenCL(const std::string& xclbin_path) {
    try {
        logInfo("Step 1: Getting OpenCL platforms...");
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        if (platforms.empty()) {
            logError("No OpenCL platforms found");
            return false;
        }
        logInfo("Found " + std::to_string(platforms.size()) + " platforms");
        
        cl::Platform platform;
        bool found_xilinx = false;
        for (auto& p : platforms) {
            std::string vendor = p.getInfo<CL_PLATFORM_VENDOR>();
            logInfo("Platform vendor: " + vendor);
            if (vendor.find("Xilinx") != std::string::npos) {
                platform = p;
                found_xilinx = true;
                break;
            }
        }
        
        if (!found_xilinx) {
            logError("No Xilinx platform found");
            return false;
        }
        logInfo("Step 2: Found Xilinx platform");
        
        logInfo("Step 3: Getting devices...");
        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
        logInfo("Found " + std::to_string(devices.size()) + " accelerator devices");
        if (devices.empty()) {
            platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
            logInfo("Found " + std::to_string(devices.size()) + " total devices");
        }
        if (devices.empty()) {
            logError("No Xilinx devices found");
            return false;
        }
        
        logInfo("Step 4: Creating context...");
        device_ = devices[0];
        
        logInfo("Step 5: Creating context object...");
        context_ = cl::Context(device_);
        
        logInfo("Step 6: Creating command queue...");
        
        cl_command_queue_properties props = CL_QUEUE_PROFILING_ENABLE;
        cl_int err;
        cl_command_queue cq = clCreateCommandQueue(context_(), device_(), props, &err);
        if (err != CL_SUCCESS || cq == nullptr) {
            logError("Failed to create command queue: error " + std::to_string(err));
            return false;
        }
        queue_ = cl::CommandQueue(cq, true);
        logInfo("Command queue created successfully");
        
        logInfo("Step 7: Getting device name...");
        std::string device_name;
        try {
            device_name = device_.getInfo<CL_DEVICE_NAME>();
            logInfo("Device: " + device_name);
        } catch (const std::exception& e) {
            logError("Failed to get device name: " + std::string(e.what()));
        }
        
        logInfo("Step 8: Loading xclbin: " + xclbin_path);
        
        auto binary = readBinaryFile(xclbin_path);
        if (binary.empty()) {
            logError("Failed to read xclbin file: " + xclbin_path);
            return false;
        }
        
        cl::Program::Binaries bins;
        bins.emplace_back(binary);
        
        std::vector<cl::Device> dev_vec = {device_};
        program_ = cl::Program(context_, dev_vec, bins);
        
        try {
            program_.build(dev_vec);
        } catch (const std::exception& e) {
            std::string build_log = program_.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device_);
            logError("OpenCL build failed: " + std::string(e.what()));
            logError("Build log: " + build_log);
            return false;
        }
        
        kernel_encrypt_ = cl::Kernel(program_, "aes256_encrypt");
        kernel_decrypt_ = cl::Kernel(program_, "aes256_decrypt");
        
        logInfo("Loaded kernels: aes256_encrypt and aes256_decrypt");
        logInfo("FPGA AES context initialized successfully");
        
        return true;
    } catch (const std::exception& e) {
        logError("Exception: " + std::string(e.what()));
        return false;
    }
}

cl_ulong FpgaAesContext::getEventDuration(cl::Event& event) {
    cl_ulong start, end;
    event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
    event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
    return end - start;
}

bool FpgaAesContext::initialize(const std::string& xclbin_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_ && xclbin_path_ == xclbin_path) {
        return true;
    }
    
    release();
    
    if (initOpenCL(xclbin_path)) {
        initialized_ = true;
        xclbin_path_ = xclbin_path;
        return true;
    }
    
    return false;
}

void FpgaAesContext::release() {
    initialized_ = false;
    xclbin_path_.clear();
}

bool FpgaAesContext::encrypt(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key,
                              FpgaAesStats* stats) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        logError("FPGA context not initialized");
        if (stats) stats->success = false;
        return false;
    }
    
    if (key.size() != 32) {
        logError("Invalid key size: expected 32 bytes, got " + std::to_string(key.size()));
        if (stats) stats->success = false;
        return false;
    }
    
    try {
        size_t block_count = (input.size() + 15) / 16;
        if (block_count == 0) block_count = 1;
        size_t padded_size = block_count * 16;
        
        std::vector<unsigned char> padded_input(padded_size, 0);
        std::memcpy(padded_input.data(), input.data(), input.size());
        
        output.resize(padded_size);
        
        cl_mem_ext_ptr_t ext_in = {};
        ext_in.flags = XCL_MEM_DDR_BANK0;
        ext_in.obj = nullptr;
        
        cl_mem_ext_ptr_t ext_out = {};
        ext_out.flags = XCL_MEM_DDR_BANK0;
        ext_out.obj = nullptr;
        
        cl_mem_ext_ptr_t ext_key = {};
        ext_key.flags = XCL_MEM_DDR_BANK1;
        ext_key.obj = nullptr;
        
        cl::Buffer buffer_in(context_, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX, padded_size, &ext_in, nullptr);
        cl::Buffer buffer_out(context_, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, padded_size, &ext_out, nullptr);
        cl::Buffer buffer_key(context_, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX, static_cast<size_t>(32), &ext_key, nullptr);
        
        cl::Event h2d_event_in, h2d_event_key, kernel_event, d2h_event;
        
        queue_.enqueueWriteBuffer(buffer_in, CL_FALSE, 0, padded_size, padded_input.data(), nullptr, &h2d_event_in);
        queue_.enqueueWriteBuffer(buffer_key, CL_FALSE, 0, 32, key.data(), nullptr, &h2d_event_key);
        queue_.finish();
        
        kernel_encrypt_.setArg(0, buffer_in);
        kernel_encrypt_.setArg(1, buffer_out);
        kernel_encrypt_.setArg(2, buffer_key);
        kernel_encrypt_.setArg(3, static_cast<unsigned int>(block_count));
        
        cl::NDRange global(1);
        cl::NDRange local(1);
        queue_.enqueueNDRangeKernel(kernel_encrypt_, cl::NullRange, global, local, nullptr, &kernel_event);
        queue_.finish();
        
        queue_.enqueueReadBuffer(buffer_out, CL_FALSE, 0, padded_size, output.data(), nullptr, &d2h_event);
        queue_.finish();
        
        if (stats) {
            stats->h2d_time_ms = (getEventDuration(h2d_event_in) + getEventDuration(h2d_event_key)) / 1e6;
            stats->kernel_time_ms = getEventDuration(kernel_event) / 1e6;
            stats->d2h_time_ms = getEventDuration(d2h_event) / 1e6;
            stats->total_time_ms = stats->h2d_time_ms + stats->kernel_time_ms + stats->d2h_time_ms;
            
            double total_bytes = block_count * 16.0;
            stats->throughput_gbps = (total_bytes / stats->kernel_time_ms) / 1e6;
            stats->success = true;
        }
        
        return true;
    } catch (const std::exception& e) {
        logError("OpenCL error during encryption: " + std::string(e.what()));
        if (stats) stats->success = false;
        return false;
    }
}

bool FpgaAesContext::decrypt(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key,
                              FpgaAesStats* stats) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        logError("FPGA context not initialized");
        if (stats) stats->success = false;
        return false;
    }
    
    if (key.size() != 32) {
        logError("Invalid key size: expected 32 bytes, got " + std::to_string(key.size()));
        if (stats) stats->success = false;
        return false;
    }
    
    try {
        size_t block_count = (input.size() + 15) / 16;
        if (block_count == 0) block_count = 1;
        size_t padded_size = block_count * 16;
        
        std::vector<unsigned char> padded_input(padded_size, 0);
        std::memcpy(padded_input.data(), input.data(), input.size());
        
        output.resize(padded_size);
        
        cl_mem_ext_ptr_t ext_in = {};
        ext_in.flags = XCL_MEM_DDR_BANK0;
        ext_in.obj = nullptr;
        
        cl_mem_ext_ptr_t ext_out = {};
        ext_out.flags = XCL_MEM_DDR_BANK0;
        ext_out.obj = nullptr;
        
        cl_mem_ext_ptr_t ext_key = {};
        ext_key.flags = XCL_MEM_DDR_BANK1;
        ext_key.obj = nullptr;
        
        cl::Buffer buffer_in(context_, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX, padded_size, &ext_in, nullptr);
        cl::Buffer buffer_out(context_, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, padded_size, &ext_out, nullptr);
        cl::Buffer buffer_key(context_, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX, static_cast<size_t>(32), &ext_key, nullptr);
        
        cl::Event h2d_event_in, h2d_event_key, kernel_event, d2h_event;
        
        queue_.enqueueWriteBuffer(buffer_in, CL_FALSE, 0, padded_size, padded_input.data(), nullptr, &h2d_event_in);
        queue_.enqueueWriteBuffer(buffer_key, CL_FALSE, 0, 32, key.data(), nullptr, &h2d_event_key);
        queue_.finish();
        
        kernel_decrypt_.setArg(0, buffer_in);
        kernel_decrypt_.setArg(1, buffer_out);
        kernel_decrypt_.setArg(2, buffer_key);
        kernel_decrypt_.setArg(3, static_cast<unsigned int>(block_count));
        
        cl::NDRange global(1);
        cl::NDRange local(1);
        queue_.enqueueNDRangeKernel(kernel_decrypt_, cl::NullRange, global, local, nullptr, &kernel_event);
        queue_.finish();
        
        queue_.enqueueReadBuffer(buffer_out, CL_FALSE, 0, padded_size, output.data(), nullptr, &d2h_event);
        queue_.finish();
        
        if (stats) {
            stats->h2d_time_ms = (getEventDuration(h2d_event_in) + getEventDuration(h2d_event_key)) / 1e6;
            stats->kernel_time_ms = getEventDuration(kernel_event) / 1e6;
            stats->d2h_time_ms = getEventDuration(d2h_event) / 1e6;
            stats->total_time_ms = stats->h2d_time_ms + stats->kernel_time_ms + stats->d2h_time_ms;
            
            double total_bytes = block_count * 16.0;
            stats->throughput_gbps = (total_bytes / stats->kernel_time_ms) / 1e6;
            stats->success = true;
        }
        
        return true;
    } catch (const std::exception& e) {
        logError("OpenCL error during decryption: " + std::string(e.what()));
        if (stats) stats->success = false;
        return false;
    }
}

std::string FpgaAesContext::getLastDeviceName() const {
    if (initialized_) {
        return device_.getInfo<CL_DEVICE_NAME>();
    }
    return "";
}

std::string FpgaAesContext::getXclbinPath() const {
    return xclbin_path_;
}

bool FpgaAesContext::isFpgaAvailable() {
    std::cerr << "[DEBUG] FpgaAes::isFpgaAvailable() called" << std::endl;
    try {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        std::cerr << "[DEBUG] Found " << platforms.size() << " OpenCL platforms" << std::endl;
        
        for (const auto& platform : platforms) {
            std::string vendor = platform.getInfo<CL_PLATFORM_VENDOR>();
            std::cerr << "[DEBUG] Platform vendor: " << vendor << std::endl;
            if (vendor.find("Xilinx") != std::string::npos) {
                std::vector<cl::Device> devices;
                platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
                std::cerr << "[DEBUG] Found " << devices.size() << " Xilinx accelerator devices" << std::endl;
                if (!devices.empty()) {
                    std::cerr << "[DEBUG] FPGA device found: " << devices[0].getInfo<CL_DEVICE_NAME>() << std::endl;
                    return true;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[DEBUG] Exception in isFpgaAvailable: " << e.what() << std::endl;
    }
    std::cerr << "[DEBUG] No FPGA device found" << std::endl;
    return false;
}

std::vector<std::string> FpgaAesContext::getAvailableDevices() {
    std::vector<std::string> devices;
    try {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        
        for (const auto& platform : platforms) {
            std::string vendor = platform.getInfo<CL_PLATFORM_VENDOR>();
            if (vendor.find("Xilinx") != std::string::npos) {
                std::vector<cl::Device> cl_devices;
                platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &cl_devices);
                for (const auto& device : cl_devices) {
                    devices.push_back(device.getInfo<CL_DEVICE_NAME>());
                }
            }
        }
    } catch (...) {
    }
    return devices;
}

#else

FpgaAesContext& FpgaAesContext::getInstance() {
    static FpgaAesContext instance;
    return instance;
}

FpgaAesContext::FpgaAesContext() : initialized_(false) {}
FpgaAesContext::~FpgaAesContext() {}

bool FpgaAesContext::initialize(const std::string&) {
    std::cerr << "[ERROR] FPGA support not compiled in. Please rebuild with USE_FPGA=1" << std::endl;
    return false;
}

void FpgaAesContext::release() {}

bool FpgaAesContext::encrypt(const std::vector<unsigned char>&,
                              std::vector<unsigned char>&,
                              const std::vector<unsigned char>&,
                              FpgaAesStats* stats) {
    std::cerr << "[ERROR] FPGA support not compiled in" << std::endl;
    if (stats) stats->success = false;
    return false;
}

bool FpgaAesContext::decrypt(const std::vector<unsigned char>&,
                              std::vector<unsigned char>&,
                              const std::vector<unsigned char>&,
                              FpgaAesStats* stats) {
    std::cerr << "[ERROR] FPGA support not compiled in" << std::endl;
    if (stats) stats->success = false;
    return false;
}

std::string FpgaAesContext::getLastDeviceName() const { return ""; }
std::string FpgaAesContext::getXclbinPath() const { return ""; }
bool FpgaAesContext::isFpgaAvailable() { return false; }
std::vector<std::string> FpgaAesContext::getAvailableDevices() { return {}; }

#endif

std::vector<unsigned char> FpgaAes::padData(const std::vector<unsigned char>& data) {
    size_t pad_len = 16 - (data.size() % 16);
    if (pad_len == 0) pad_len = 16;
    
    std::vector<unsigned char> padded = data;
    padded.insert(padded.end(), pad_len, static_cast<unsigned char>(pad_len));
    return padded;
}

std::vector<unsigned char> FpgaAes::unpadData(const std::vector<unsigned char>& data) {
    if (data.empty()) return {};
    
    unsigned char pad_len = data.back();
    if (pad_len > 16 || pad_len == 0) return data;
    
    for (size_t i = data.size() - pad_len; i < data.size(); i++) {
        if (data[i] != pad_len) return data;
    }
    
    return std::vector<unsigned char>(data.begin(), data.end() - pad_len);
}

bool FpgaAes::encrypt(const std::vector<unsigned char>& input,
                       std::vector<unsigned char>& output,
                       const std::vector<unsigned char>& key,
                       const std::string& xclbin_path,
                       FpgaAesStats* stats) {
    std::cerr << "[DEBUG] FpgaAes::encrypt() called, input size: " << input.size() << " bytes" << std::endl;
    auto& ctx = FpgaAesContext::getInstance();
    
    std::string xclbin = xclbin_path.empty() ? getDefaultXclbinPath() : xclbin_path;
    std::cerr << "[DEBUG] Using xclbin: " << xclbin << std::endl;
    
    if (!ctx.isInitialized()) {
        std::cerr << "[DEBUG] FPGA context not initialized, initializing now..." << std::endl;
        if (!ctx.initialize(xclbin)) {
            std::cerr << "[DEBUG] FPGA context initialization FAILED" << std::endl;
            if (stats) stats->success = false;
            return false;
        }
        std::cerr << "[DEBUG] FPGA context initialized successfully" << std::endl;
    }
    
    std::vector<unsigned char> padded = padData(input);
    std::cerr << "[DEBUG] Padded input size: " << padded.size() << " bytes" << std::endl;
    bool result = ctx.encrypt(padded, output, key, stats);
    std::cerr << "[DEBUG] FPGA encrypt result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    return result;
}

bool FpgaAes::decrypt(const std::vector<unsigned char>& input,
                       std::vector<unsigned char>& output,
                       const std::vector<unsigned char>& key,
                       const std::string& xclbin_path,
                       FpgaAesStats* stats) {
    std::cerr << "[DEBUG] FpgaAes::decrypt() called, input size: " << input.size() << " bytes" << std::endl;
    auto& ctx = FpgaAesContext::getInstance();
    
    std::string xclbin = xclbin_path.empty() ? getDefaultXclbinPath() : xclbin_path;
    std::cerr << "[DEBUG] Using xclbin: " << xclbin << std::endl;
    
    if (!ctx.isInitialized()) {
        std::cerr << "[DEBUG] FPGA context not initialized, initializing now..." << std::endl;
        if (!ctx.initialize(xclbin)) {
            std::cerr << "[DEBUG] FPGA context initialization FAILED" << std::endl;
            if (stats) stats->success = false;
            return false;
        }
        std::cerr << "[DEBUG] FPGA context initialized successfully" << std::endl;
    }
    
    std::vector<unsigned char> decrypted;
    if (!ctx.decrypt(input, decrypted, key, stats)) {
        std::cerr << "[DEBUG] FPGA decrypt FAILED" << std::endl;
        return false;
    }
    
    output = unpadData(decrypted);
    std::cerr << "[DEBUG] FPGA decrypt SUCCESS, output size: " << output.size() << " bytes" << std::endl;
    return true;
}

bool FpgaAes::isAvailable() {
    return FpgaAesContext::isFpgaAvailable();
}

std::string FpgaAes::getDefaultXclbinPath() {
#ifdef XCLBIN_PATH
    return XCLBIN_PATH;
#else
    const char* env_path = std::getenv("DFS_XCLBIN_PATH");
    if (env_path && env_path[0] != '\0') {
        return std::string(env_path);
    }
    return "/home/zhuzh/data/test/aes-hls-fpga/build/aes256.hw.xclbin";
#endif
}

} // namespace crypto
} // namespace dfs
