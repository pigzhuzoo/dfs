#include "../include/crypto_utils.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <openssl/sha.h>

namespace dfs {
namespace crypto {

// CryptoContext构造函数
CryptoContext::CryptoContext(EncryptionAlgorithm algo, const std::vector<unsigned char>& key)
    : algorithm_(algo), key_(key), cipher_ctx_(nullptr), rsa_key_(nullptr), valid_(false) {
    
    switch (algorithm_) {
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::AES_256_ECB:
        case EncryptionAlgorithm::SM4_CTR:
            cipher_ctx_ = EVP_CIPHER_CTX_new();
            if (!cipher_ctx_) {
                // 直接输出错误信息而不是调用私有函数
                unsigned long err = ERR_get_error();
                if (err != 0) {
                    char err_buf[256];
                    ERR_error_string_n(err, err_buf, sizeof(err_buf));
                    std::cerr << "OpenSSL error in EVP_CIPHER_CTX_new: " << err_buf << std::endl;
                } else {
                    std::cerr << "OpenSSL error in EVP_CIPHER_CTX_new: Unknown error" << std::endl;
                }
                return;
            }
            valid_ = true;
            break;
            
        case EncryptionAlgorithm::RSA_OAEP:
            // RSA密钥处理将在具体操作时进行
            valid_ = !key.empty();
            break;
            
        default:
            // 默认情况下，如果算法不支持，设置为无效
            valid_ = false;
            break;
    }
}

// CryptoContext析构函数
CryptoContext::~CryptoContext() {
    if (cipher_ctx_) {
        EVP_CIPHER_CTX_free(cipher_ctx_);
    }
    if (rsa_key_) {
        RSA_free(rsa_key_);
    }
}

// CryptoContext移动构造函数
CryptoContext::CryptoContext(CryptoContext&& other) noexcept
    : algorithm_(other.algorithm_), 
      key_(std::move(other.key_)),
      cipher_ctx_(other.cipher_ctx_),
      rsa_key_(other.rsa_key_),
      valid_(other.valid_) {
    other.cipher_ctx_ = nullptr;
    other.rsa_key_ = nullptr;
    other.valid_ = false;
}

// CryptoContext移动赋值运算符
CryptoContext& CryptoContext::operator=(CryptoContext&& other) noexcept {
    if (this != &other) {
        if (cipher_ctx_) {
            EVP_CIPHER_CTX_free(cipher_ctx_);
        }
        if (rsa_key_) {
            RSA_free(rsa_key_);
        }
        
        algorithm_ = other.algorithm_;
        key_ = std::move(other.key_);
        cipher_ctx_ = other.cipher_ctx_;
        rsa_key_ = other.rsa_key_;
        valid_ = other.valid_;
        
        other.cipher_ctx_ = nullptr;
        other.rsa_key_ = nullptr;
        other.valid_ = false;
    }
    return *this;
}

// AES-256-GCM加密
bool CryptoUtils::aes256GcmEncrypt(const std::vector<unsigned char>& input,
                                  std::vector<unsigned char>& output,
                                  const std::vector<unsigned char>& key) {
    if (key.size() != 32) { // AES-256需要32字节密钥
        std::cerr << "Invalid AES-256 key size: " << key.size() << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("EVP_CIPHER_CTX_new");
        return false;
    }
    
    // 初始化加密上下文
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        handleOpenSSLError("EVP_EncryptInit_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 设置密钥长度
    if (EVP_CIPHER_CTX_set_key_length(ctx, 32) != 1) {
        handleOpenSSLError("EVP_CIPHER_CTX_set_key_length");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 生成随机IV
    unsigned char iv[12];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        handleOpenSSLError("RAND_bytes");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 设置密钥和IV
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) != 1) {
        handleOpenSSLError("EVP_EncryptInit_ex (key/iv)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 分配输出缓冲区（输入大小 + IV + 标签 + 填充）
    output.resize(input.size() + sizeof(iv) + 16 + EVP_MAX_BLOCK_LENGTH);
    
    // 写入IV到输出的开头
    std::memcpy(output.data(), iv, sizeof(iv));
    size_t out_offset = sizeof(iv);
    
    int len;
    // 加密数据
    if (EVP_EncryptUpdate(ctx, output.data() + out_offset, &len, input.data(), input.size()) != 1) {
        handleOpenSSLError("EVP_EncryptUpdate");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_offset += len;
    
    // 完成加密
    if (EVP_EncryptFinal_ex(ctx, output.data() + out_offset, &len) != 1) {
        handleOpenSSLError("EVP_EncryptFinal_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_offset += len;
    
    // 获取认证标签
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, output.data() + out_offset) != 1) {
        handleOpenSSLError("EVP_CIPHER_CTX_ctrl (get tag)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_offset += 16;
    
    // 调整输出大小
    output.resize(out_offset);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// AES-256-ECB加密
bool CryptoUtils::aes256EcbEncrypt(const std::vector<unsigned char>& input,
                                  std::vector<unsigned char>& output,
                                  const std::vector<unsigned char>& key) {
    if (key.size() != 32) { // AES-256需要32字节密钥
        std::cerr << "Invalid AES-256 key size: " << key.size() << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("EVP_CIPHER_CTX_new");
        return false;
    }
    
    // 初始化ECB加密上下文
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key.data(), nullptr) != 1) {
        handleOpenSSLError("EVP_EncryptInit_ex (ECB)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 启用PKCS7填充（默认值，但显式设置以明确意图）
    if (EVP_CIPHER_CTX_set_padding(ctx, 1) != 1) {
        handleOpenSSLError("EVP_CIPHER_CTX_set_padding");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 分配输出缓冲区（输入大小 + 最大可能的填充）
    output.resize(input.size() + 16);
    
    int len;
    int out_offset = 0;
    
    // 加密数据
    if (EVP_EncryptUpdate(ctx, output.data(), &len, input.data(), input.size()) != 1) {
        handleOpenSSLError("EVP_EncryptUpdate (ECB)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_offset += len;
    
    // 完成加密（会自动添加PKCS7填充）
    if (EVP_EncryptFinal_ex(ctx, output.data() + out_offset, &len) != 1) {
        handleOpenSSLError("EVP_EncryptFinal_ex (ECB)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_offset += len;
    
    // 调整输出大小
    output.resize(out_offset);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// AES-256-GCM解密
bool CryptoUtils::aes256GcmDecrypt(const std::vector<unsigned char>& input,
                                  std::vector<unsigned char>& output,
                                  const std::vector<unsigned char>& key) {
    if (key.size() != 32) {
        std::cerr << "Invalid AES-256 key size: " << key.size() << std::endl;
        return false;
    }
    
    if (input.size() < 12 + 16) { // 至少需要IV(12) + 标签(16)
        std::cerr << "Input too small for AES-256-GCM decryption" << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("EVP_CIPHER_CTX_new");
        return false;
    }
    
    // 初始化解密上下文
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        handleOpenSSLError("EVP_DecryptInit_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 设置密钥长度
    if (EVP_CIPHER_CTX_set_key_length(ctx, 32) != 1) {
        handleOpenSSLError("EVP_CIPHER_CTX_set_key_length");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 提取IV
    unsigned char iv[12];
    std::memcpy(iv, input.data(), sizeof(iv));
    
    // 设置密钥和IV
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) != 1) {
        handleOpenSSLError("EVP_DecryptInit_ex (key/iv)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 提取认证标签
    unsigned char tag[16];
    std::memcpy(tag, input.data() + input.size() - 16, 16);
    
    // 设置认证标签
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1) {
        handleOpenSSLError("EVP_CIPHER_CTX_ctrl (set tag)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 分配输出缓冲区
    output.resize(input.size() - sizeof(iv) - 16);
    
    int len;
    // 解密数据
    if (EVP_DecryptUpdate(ctx, output.data(), &len, input.data() + sizeof(iv), 
                          input.size() - sizeof(iv) - 16) != 1) {
        handleOpenSSLError("EVP_DecryptUpdate");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    int out_len = len;
    
    // 完成解密并验证标签
    if (EVP_DecryptFinal_ex(ctx, output.data() + out_len, &len) != 1) {
        handleOpenSSLError("EVP_DecryptFinal_ex (authentication failed)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_len += len;
    
    output.resize(out_len);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// AES-256-ECB解密
bool CryptoUtils::aes256EcbDecrypt(const std::vector<unsigned char>& input,
                                  std::vector<unsigned char>& output,
                                  const std::vector<unsigned char>& key) {
    if (key.size() != 32) { // AES-256需要32字节密钥
        std::cerr << "Invalid AES-256 key size: " << key.size() << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("EVP_CIPHER_CTX_new");
        return false;
    }
    
    // 初始化ECB解密上下文
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key.data(), nullptr) != 1) {
        handleOpenSSLError("EVP_DecryptInit_ex (ECB)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 启用PKCS7填充（默认就是启用的，会自动移除填充）
    // EVP_CIPHER_CTX_set_padding(ctx, 1); // 默认值，可以省略
    
    // 分配输出缓冲区（最大可能大小）
    output.resize(input.size());
    
    int len;
    int out_len = 0;
    
    // 解密数据
    if (EVP_DecryptUpdate(ctx, output.data(), &len, input.data(), input.size()) != 1) {
        handleOpenSSLError("EVP_DecryptUpdate (ECB)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_len += len;
    
    // 完成解密（会自动移除PKCS7填充）
    if (EVP_DecryptFinal_ex(ctx, output.data() + out_len, &len) != 1) {
        handleOpenSSLError("EVP_DecryptFinal_ex (ECB padding error)");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_len += len;
    
    // 调整输出大小
    output.resize(out_len);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// SM4-CTR加密（如果OpenSSL版本支持）
bool CryptoUtils::sm4CtrEncrypt(const std::vector<unsigned char>& input,
                               std::vector<unsigned char>& output,
                               const std::vector<unsigned char>& key) {
#ifdef OPENSSL_NO_SM4
    std::cerr << "SM4 not supported in this OpenSSL build" << std::endl;
    return false;
#else
    if (key.size() != 16) { // SM4需要16字节密钥
        std::cerr << "Invalid SM4 key size: " << key.size() << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("EVP_CIPHER_CTX_new");
        return false;
    }
    
    // 生成随机IV
    unsigned char iv[16];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        handleOpenSSLError("RAND_bytes");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 初始化SM4-CTR加密
    if (EVP_EncryptInit_ex(ctx, EVP_sm4_ctr(), nullptr, key.data(), iv) != 1) {
        handleOpenSSLError("EVP_EncryptInit_ex SM4-CTR");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 分配输出缓冲区（输入大小 + IV + 填充）
    output.resize(input.size() + sizeof(iv) + EVP_MAX_BLOCK_LENGTH);
    
    // 写入IV到输出的开头
    std::memcpy(output.data(), iv, sizeof(iv));
    size_t out_offset = sizeof(iv);
    
    int len;
    // 加密数据
    if (EVP_EncryptUpdate(ctx, output.data() + out_offset, &len, input.data(), input.size()) != 1) {
        handleOpenSSLError("EVP_EncryptUpdate SM4");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_offset += len;
    
    // 完成加密
    if (EVP_EncryptFinal_ex(ctx, output.data() + out_offset, &len) != 1) {
        handleOpenSSLError("EVP_EncryptFinal_ex SM4");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    out_offset += len;
    
    output.resize(out_offset);
    EVP_CIPHER_CTX_free(ctx);
    return true;
#endif
}

// SM4-CTR解密
bool CryptoUtils::sm4CtrDecrypt(const std::vector<unsigned char>& input,
                               std::vector<unsigned char>& output,
                               const std::vector<unsigned char>& key) {
#ifdef OPENSSL_NO_SM4
    std::cerr << "SM4 not supported in this OpenSSL build" << std::endl;
    return false;
#else
    if (key.size() != 16 || input.size() < 16) {
        std::cerr << "Invalid input for SM4-CTR decryption" << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("EVP_CIPHER_CTX_new");
        return false;
    }
    
    // 提取IV
    unsigned char iv[16];
    std::memcpy(iv, input.data(), sizeof(iv));
    
    // 初始化SM4-CTR解密
    if (EVP_DecryptInit_ex(ctx, EVP_sm4_ctr(), nullptr, key.data(), iv) != 1) {
        handleOpenSSLError("EVP_DecryptInit_ex SM4-CTR");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 分配输出缓冲区
    output.resize(input.size() - 16);
    
    int len;
    // 解密数据
    if (EVP_DecryptUpdate(ctx, output.data(), &len, input.data() + 16, input.size() - 16) != 1) {
        handleOpenSSLError("EVP_DecryptUpdate SM4");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // 完成解密
    if (EVP_DecryptFinal_ex(ctx, output.data() + len, &len) != 1) {
        handleOpenSSLError("EVP_DecryptFinal_ex SM4");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    output.resize(len);
    EVP_CIPHER_CTX_free(ctx);
    return true;
#endif
}

// XOR加密（向后兼容）
bool CryptoUtils::xorEncrypt(const std::vector<unsigned char>& input,
                            std::vector<unsigned char>& output,
                            const std::vector<unsigned char>& key) {
    if (key.empty()) {
        std::cerr << "Empty key for XOR encryption" << std::endl;
        return false;
    }
    
    output.resize(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ key[i % key.size()];
    }
    return true;
}

// XOR解密（与加密相同）
bool CryptoUtils::xorDecrypt(const std::vector<unsigned char>& input,
                            std::vector<unsigned char>& output,
                            const std::vector<unsigned char>& key) {
    return xorEncrypt(input, output, key);
}

// 文件加密
bool CryptoUtils::encryptFile(const std::string& input_file, 
                             const std::string& output_file,
                             EncryptionAlgorithm algorithm,
                             const std::vector<unsigned char>& key) {
    // 读取输入文件
    std::ifstream infile(input_file, std::ios::binary);
    if (!infile) {
        std::cerr << "Cannot open input file: " << input_file << std::endl;
        return false;
    }
    
    std::vector<unsigned char> input_data((std::istreambuf_iterator<char>(infile)),
                                         std::istreambuf_iterator<char>());
    infile.close();
    
    if (input_data.empty()) {
        // 创建空输出文件
        std::ofstream outfile(output_file, std::ios::binary);
        return outfile.good();
    }
    
    std::vector<unsigned char> output_data;
    bool success = encryptData(input_data, output_data, algorithm, key);
    
    if (!success) {
        return false;
    }
    
    // 写入输出文件
    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile) {
        std::cerr << "Cannot open output file: " << output_file << std::endl;
        return false;
    }
    
    outfile.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());
    outfile.close();
    
    return true;
}

// 文件解密
bool CryptoUtils::decryptFile(const std::string& input_file,
                             const std::string& output_file,
                             EncryptionAlgorithm algorithm,
                             const std::vector<unsigned char>& key) {
    // 读取输入文件
    std::ifstream infile(input_file, std::ios::binary);
    if (!infile) {
        std::cerr << "Cannot open input file: " << input_file << std::endl;
        return false;
    }
    
    std::vector<unsigned char> input_data((std::istreambuf_iterator<char>(infile)),
                                         std::istreambuf_iterator<char>());
    infile.close();
    
    if (input_data.empty()) {
        // 创建空输出文件
        std::ofstream outfile(output_file, std::ios::binary);
        return outfile.good();
    }
    
    std::vector<unsigned char> output_data;
    bool success = decryptData(input_data, output_data, algorithm, key);
    
    if (!success) {
        return false;
    }
    
    // 写入输出文件
    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile) {
        std::cerr << "Cannot open output file: " << output_file << std::endl;
        return false;
    }
    
    outfile.write(reinterpret_cast<const char*>(output_data.data()), output_data.size());
    outfile.close();
    
    return true;
}

// 内存数据加密
bool CryptoUtils::encryptData(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             EncryptionAlgorithm algorithm,
                             const std::vector<unsigned char>& key) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
            return aes256GcmEncrypt(input, output, key);
            
        case EncryptionAlgorithm::AES_256_ECB:
            return aes256EcbEncrypt(input, output, key);
            
        case EncryptionAlgorithm::SM4_CTR:
            return sm4CtrEncrypt(input, output, key);
            
        case EncryptionAlgorithm::RSA_OAEP:
            // RSA不适合大文件加密，主要用于密钥加密
            std::cerr << "RSA-OAEP not suitable for large data encryption" << std::endl;
            return false;
            
        default:
            std::cerr << "Unsupported encryption algorithm" << std::endl;
            return false;
    }
}

// 内存数据解密
bool CryptoUtils::decryptData(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             EncryptionAlgorithm algorithm,
                             const std::vector<unsigned char>& key) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
            return aes256GcmDecrypt(input, output, key);
            
        case EncryptionAlgorithm::AES_256_ECB:
            return aes256EcbDecrypt(input, output, key);
            
        case EncryptionAlgorithm::SM4_CTR:
            return sm4CtrDecrypt(input, output, key);
            
        case EncryptionAlgorithm::RSA_OAEP:
            std::cerr << "RSA-OAEP not suitable for large data decryption" << std::endl;
            return false;
            
        default:
            std::cerr << "Unsupported decryption algorithm" << std::endl;
            return false;
    }
}

// 从密码生成密钥
std::vector<unsigned char> CryptoUtils::generateKeyFromPassword(const std::string& password,
                                                              EncryptionAlgorithm algorithm) {
    std::vector<unsigned char> password_bytes(password.begin(), password.end());
    std::vector<unsigned char> hash = sha256Hash(password_bytes);
    
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
            // AES-256需要32字节，SHA256正好是32字节
            return hash;
            
        case EncryptionAlgorithm::AES_256_ECB:
            // AES-256需要32字节，SHA256正好是32字节
            return hash;
            
        case EncryptionAlgorithm::SM4_CTR:
            // SM4需要16字节，取SHA256的前16字节
            return std::vector<unsigned char>(hash.begin(), hash.begin() + 16);
            
        case EncryptionAlgorithm::RSA_OAEP:
            // RSA使用完整的哈希作为输入
            return hash;
            
        default:
            // 默认返回SHA256哈希（适用于AES）
            return hash;
    }
}

// 获取算法名称
std::string CryptoUtils::getAlgorithmName(EncryptionAlgorithm algorithm) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
            return "AES-256-GCM";
        case EncryptionAlgorithm::AES_256_ECB:
            return "AES-256-ECB";
        case EncryptionAlgorithm::SM4_CTR:
            return "SM4-CTR";
        case EncryptionAlgorithm::RSA_OAEP:
            return "RSA-OAEP";
        default:
            return "AES-256-GCM";
    }
}

// 检查算法是否可用
bool CryptoUtils::isAlgorithmSupported(EncryptionAlgorithm algorithm) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::AES_256_ECB:
            return true; // AES总是可用的
            
        case EncryptionAlgorithm::SM4_CTR:
#ifndef OPENSSL_NO_SM4
            return true;
#else
            return false;
#endif
            
        case EncryptionAlgorithm::RSA_OAEP:
            return true; // RSA通常可用
            
        default:
            return false;
    }
}

// 辅助函数：处理OpenSSL错误
void CryptoUtils::handleOpenSSLError(const char* operation) {
    unsigned long err = ERR_get_error();
    if (err != 0) {
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        std::cerr << "OpenSSL error in " << operation << ": " << err_buf << std::endl;
    } else {
        std::cerr << "OpenSSL error in " << operation << ": Unknown error" << std::endl;
    }
}

// 辅助函数：SHA256哈希
std::vector<unsigned char> CryptoUtils::sha256Hash(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

} // namespace crypto
} // namespace dfs