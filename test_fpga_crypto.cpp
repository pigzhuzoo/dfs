#include "crypto/fpga_aes.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>

using namespace dfs::crypto;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    FPGA AES Encryption/Decryption Test" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Check FPGA availability
    std::cout << "\nChecking FPGA availability..." << std::endl;
    if (!FpgaAes::isAvailable()) {
        std::cerr << "FPGA not available!" << std::endl;
        return 1;
    }
    std::cout << "✓ FPGA is available" << std::endl;
    
    // Get available devices
    auto devices = FpgaAesContext::getAvailableDevices();
    std::cout << "\nAvailable FPGA devices:" << std::endl;
    for (const auto& dev : devices) {
        std::cout << "  - " << dev << std::endl;
    }
    
    // Test data
    std::string plaintext_str = "Hello, this is a test message for FPGA AES encryption!";
    std::vector<unsigned char> plaintext(plaintext_str.begin(), plaintext_str.end());
    std::vector<unsigned char> key(32, 0x01);  // 256-bit key
    std::vector<unsigned char> encrypted;
    std::vector<unsigned char> decrypted;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing Encryption" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Plaintext: " << plaintext_str << std::endl;
    std::cout << "Plaintext size: " << plaintext.size() << " bytes" << std::endl;
    std::cout << "XCLBIN: " << FpgaAes::getDefaultXclbinPath() << std::endl;
    
    FpgaAesStats enc_stats;
    if (!FpgaAes::encrypt(plaintext, encrypted, key, "", &enc_stats)) {
        std::cerr << "Encryption failed!" << std::endl;
        return 1;
    }
    
    std::cout << "✓ Encryption successful" << std::endl;
    std::cout << "  Encrypted size: " << encrypted.size() << " bytes" << std::endl;
    std::cout << "  H2D time: " << enc_stats.h2d_time_ms << " ms" << std::endl;
    std::cout << "  Kernel time: " << enc_stats.kernel_time_ms << " ms" << std::endl;
    std::cout << "  D2H time: " << enc_stats.d2h_time_ms << " ms" << std::endl;
    std::cout << "  Total FPGA time: " << enc_stats.total_time_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << enc_stats.throughput_gbps << " GB/s" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing Decryption" << std::endl;
    std::cout << "========================================" << std::endl;
    
    FpgaAesStats dec_stats;
    if (!FpgaAes::decrypt(encrypted, decrypted, key, "", &dec_stats)) {
        std::cerr << "Decryption failed!" << std::endl;
        return 1;
    }
    
    std::cout << "✓ Decryption successful" << std::endl;
    std::cout << "  Decrypted size: " << decrypted.size() << " bytes" << std::endl;
    std::cout << "  H2D time: " << dec_stats.h2d_time_ms << " ms" << std::endl;
    std::cout << "  Kernel time: " << dec_stats.kernel_time_ms << " ms" << std::endl;
    std::cout << "  D2H time: " << dec_stats.d2h_time_ms << " ms" << std::endl;
    std::cout << "  Total FPGA time: " << dec_stats.total_time_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << dec_stats.throughput_gbps << " GB/s" << std::endl;
    
    // Verify decrypted data
    std::string decrypted_str(decrypted.begin(), decrypted.end());
    std::cout << "\n========================================" << std::endl;
    std::cout << "Verification" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Decrypted: " << decrypted_str << std::endl;
    
    if (plaintext == decrypted) {
        std::cout << "✓ Data verification PASSED!" << std::endl;
    } else {
        std::cerr << "✗ Data verification FAILED!" << std::endl;
        std::cerr << "  Expected: " << plaintext_str << std::endl;
        std::cerr << "  Got: " << decrypted_str << std::endl;
        return 1;
    }
    
    // Test with larger data
    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing with 1MB data" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::vector<unsigned char> large_data(1024 * 1024);
    for (size_t i = 0; i < large_data.size(); i++) {
        large_data[i] = (unsigned char)(i % 256);
    }
    
    std::vector<unsigned char> large_encrypted;
    std::vector<unsigned char> large_decrypted;
    
    FpgaAesStats large_enc_stats;
    if (!FpgaAes::encrypt(large_data, large_encrypted, key, "", &large_enc_stats)) {
        std::cerr << "Large data encryption failed!" << std::endl;
        return 1;
    }
    
    std::cout << "✓ Large data encryption successful" << std::endl;
    std::cout << "  Data size: " << large_data.size() / 1024 / 1024 << " MB" << std::endl;
    std::cout << "  Kernel time: " << large_enc_stats.kernel_time_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << large_enc_stats.throughput_gbps << " GB/s" << std::endl;
    
    FpgaAesStats large_dec_stats;
    if (!FpgaAes::decrypt(large_encrypted, large_decrypted, key, "", &large_dec_stats)) {
        std::cerr << "Large data decryption failed!" << std::endl;
        return 1;
    }
    
    if (large_data == large_decrypted) {
        std::cout << "✓ Large data verification PASSED!" << std::endl;
    } else {
        std::cerr << "✗ Large data verification FAILED!" << std::endl;
        return 1;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
