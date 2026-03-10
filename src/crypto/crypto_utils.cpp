#include "crypto_utils.hpp"
#include "crypto/fpga_aes.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace dfs {
namespace crypto {

CryptoContext::CryptoContext(EncryptionAlgorithm algo, const std::vector<unsigned char>& key)
    : algorithm_(algo), key_(key), cipher_ctx_(nullptr), rsa_key_(nullptr), valid_(false) {
    
    if (algo == EncryptionAlgorithm::RSA_OAEP) {
        // RSA key generation would go here
        // For now, we'll mark it as invalid
        valid_ = false;
        return;
    }
    
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    if (cipher_ctx_) {
        valid_ = true;
    }
}

CryptoContext::~CryptoContext() {
    if (cipher_ctx_) {
        EVP_CIPHER_CTX_free(cipher_ctx_);
    }
    if (rsa_key_) {
        RSA_free(rsa_key_);
    }
}

CryptoContext::CryptoContext(CryptoContext&& other) noexcept
    : algorithm_(other.algorithm_)
    , key_(std::move(other.key_))
    , cipher_ctx_(other.cipher_ctx_)
    , rsa_key_(other.rsa_key_)
    , valid_(other.valid_) {
    other.cipher_ctx_ = nullptr;
    other.rsa_key_ = nullptr;
    other.valid_ = false;
}

CryptoContext& CryptoContext::operator=(CryptoContext&& other) noexcept {
    if (this != &other) {
        if (cipher_ctx_) EVP_CIPHER_CTX_free(cipher_ctx_);
        if (rsa_key_) RSA_free(rsa_key_);
        
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

void CryptoUtils::handleOpenSSLError(const char* operation) {
    unsigned long err;
    while ((err = ERR_get_error())) {
        char err_msg[256];
        ERR_error_string_n(err, err_msg, sizeof(err_msg));
        std::cerr << "[OpenSSL Error] " << operation << ": " << err_msg << std::endl;
    }
}

std::vector<unsigned char> CryptoUtils::sha256Hash(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> hash(32);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        handleOpenSSLError("sha256Hash");
        return {};
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash.data(), nullptr) != 1) {
        handleOpenSSLError("sha256Hash");
        EVP_MD_CTX_free(ctx);
        return {};
    }
    
    EVP_MD_CTX_free(ctx);
    return hash;
}

std::string CryptoUtils::getAlgorithmName(EncryptionAlgorithm algorithm) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM: return "AES-256-GCM";
        case EncryptionAlgorithm::AES_256_ECB: return "AES-256-ECB";
        case EncryptionAlgorithm::AES_256_CBC: return "AES-256-CBC";
        case EncryptionAlgorithm::AES_256_CFB: return "AES-256-CFB";
        case EncryptionAlgorithm::AES_256_OFB: return "AES-256-OFB";
        case EncryptionAlgorithm::AES_256_CTR: return "AES-256-CTR";
        case EncryptionAlgorithm::SM4_ECB: return "SM4-ECB";
        case EncryptionAlgorithm::SM4_CBC: return "SM4-CBC";
        case EncryptionAlgorithm::SM4_CTR: return "SM4-CTR";
        case EncryptionAlgorithm::RSA_OAEP: return "RSA-OAEP";
        case EncryptionAlgorithm::AES_256_FPGA: return "AES-256-FPGA";
        default: return "Unknown";
    }
}

int CryptoUtils::getKeySize(EncryptionAlgorithm algorithm) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::AES_256_ECB:
        case EncryptionAlgorithm::AES_256_CBC:
        case EncryptionAlgorithm::AES_256_CFB:
        case EncryptionAlgorithm::AES_256_OFB:
        case EncryptionAlgorithm::AES_256_CTR:
        case EncryptionAlgorithm::AES_256_FPGA:
            return 32; // 256 bits
        case EncryptionAlgorithm::SM4_ECB:
        case EncryptionAlgorithm::SM4_CBC:
        case EncryptionAlgorithm::SM4_CTR:
            return 16; // 128 bits
        case EncryptionAlgorithm::RSA_OAEP:
            return 256; // 2048 bits minimum
        default:
            return 0;
    }
}

int CryptoUtils::getIVSize(EncryptionAlgorithm algorithm) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::AES_256_CBC:
        case EncryptionAlgorithm::AES_256_CFB:
        case EncryptionAlgorithm::AES_256_OFB:
        case EncryptionAlgorithm::AES_256_CTR:
            return 16; // 128 bits
        case EncryptionAlgorithm::SM4_ECB:
        case EncryptionAlgorithm::AES_256_ECB:
        case EncryptionAlgorithm::AES_256_FPGA:
            return 0;
        case EncryptionAlgorithm::SM4_CBC:
        case EncryptionAlgorithm::SM4_CTR:
            return 16; // 128 bits
        default:
            return 16;
    }
}

bool CryptoUtils::isAlgorithmSupported(EncryptionAlgorithm algorithm) {
    const EVP_CIPHER* cipher = nullptr;
    
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
            cipher = EVP_aes_256_gcm();
            break;
        case EncryptionAlgorithm::AES_256_ECB:
            cipher = EVP_aes_256_ecb();
            break;
        case EncryptionAlgorithm::AES_256_CBC:
            cipher = EVP_aes_256_cbc();
            break;
        case EncryptionAlgorithm::AES_256_CFB:
            cipher = EVP_aes_256_cfb8();
            break;
        case EncryptionAlgorithm::AES_256_OFB:
            cipher = EVP_aes_256_ofb();
            break;
        case EncryptionAlgorithm::AES_256_CTR:
            cipher = EVP_aes_256_ctr();
            break;
        case EncryptionAlgorithm::SM4_ECB:
            cipher = EVP_sm4_ecb();
            break;
        case EncryptionAlgorithm::SM4_CBC:
            cipher = EVP_sm4_cbc();
            break;
        case EncryptionAlgorithm::SM4_CTR:
            cipher = EVP_sm4_ctr();
            break;
        case EncryptionAlgorithm::RSA_OAEP:
            return true;
        case EncryptionAlgorithm::AES_256_FPGA:
            return FpgaAes::isAvailable();
        default:
            return false;
    }
    
    return cipher != nullptr;
}

std::vector<unsigned char> CryptoUtils::generateKeyFromPassword(const std::string& password,
                                                                EncryptionAlgorithm algorithm) {
    int key_size = getKeySize(algorithm);
    std::vector<unsigned char> key(key_size);
    
    std::vector<unsigned char> password_bytes(password.begin(), password.end());
    std::vector<unsigned char> hash = sha256Hash(password_bytes);
    
    if (hash.empty()) {
        std::fill(key.begin(), key.end(), 0);
        return key;
    }
    
    if (key_size <= 32) {
        std::copy(hash.begin(), hash.begin() + key_size, key.begin());
    } else {
        for (int i = 0; i < key_size; i++) {
            key[i] = hash[i % 32];
        }
    }
    
    return key;
}

bool CryptoUtils::genericEncrypt(const EVP_CIPHER* cipher,
                                 const std::vector<unsigned char>& input,
                                 std::vector<unsigned char>& output,
                                 const std::vector<unsigned char>& key,
                                 int iv_size,
                                 bool needs_padding,
                                 bool needs_iv) {
    if (!cipher) {
        std::cerr << "Cipher not supported" << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("genericEncrypt");
        return false;
    }
    
    std::vector<unsigned char> iv;
    if (needs_iv && iv_size > 0) {
        iv.resize(iv_size);
        if (RAND_bytes(iv.data(), iv_size) != 1) {
            handleOpenSSLError("RAND_bytes");
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
    }
    
    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), needs_iv ? iv.data() : nullptr) != 1) {
        handleOpenSSLError("EVP_EncryptInit_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (!needs_padding) {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
    }
    
    output.clear();
    if (needs_iv && iv_size > 0) {
        output.insert(output.end(), iv.begin(), iv.end());
    }
    
    std::vector<unsigned char> temp(input.size() + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    int ciphertext_len = 0;
    
    if (EVP_EncryptUpdate(ctx, temp.data(), &len, input.data(), input.size()) != 1) {
        handleOpenSSLError("EVP_EncryptUpdate");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, temp.data() + len, &len) != 1) {
        handleOpenSSLError("EVP_EncryptFinal_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    ciphertext_len += len;
    
    output.insert(output.end(), temp.begin(), temp.begin() + ciphertext_len);
    
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

bool CryptoUtils::genericDecrypt(const EVP_CIPHER* cipher,
                                 const std::vector<unsigned char>& input,
                                 std::vector<unsigned char>& output,
                                 const std::vector<unsigned char>& key,
                                 int iv_size,
                                 bool needs_padding,
                                 bool needs_iv) {
    if (!cipher) {
        std::cerr << "Cipher not supported" << std::endl;
        return false;
    }
    
    if (needs_iv && input.size() < (size_t)iv_size) {
        std::cerr << "Input too short" << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("genericDecrypt");
        return false;
    }
    
    const unsigned char* iv_ptr = nullptr;
    const unsigned char* ciphertext = input.data();
    size_t ciphertext_len = input.size();
    
    if (needs_iv && iv_size > 0) {
        iv_ptr = input.data();
        ciphertext = input.data() + iv_size;
        ciphertext_len = input.size() - iv_size;
    }
    
    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), iv_ptr) != 1) {
        handleOpenSSLError("EVP_DecryptInit_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (!needs_padding) {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
    }
    
    output.clear();
    std::vector<unsigned char> temp(ciphertext_len + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    int plaintext_len = 0;
    
    if (EVP_DecryptUpdate(ctx, temp.data(), &len, ciphertext, ciphertext_len) != 1) {
        handleOpenSSLError("EVP_DecryptUpdate");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len = len;
    
    if (EVP_DecryptFinal_ex(ctx, temp.data() + len, &len) != 1) {
        handleOpenSSLError("EVP_DecryptFinal_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len += len;
    
    output.assign(temp.begin(), temp.begin() + plaintext_len);
    
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

bool CryptoUtils::aes256GcmEncrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("aes256GcmEncrypt");
        return false;
    }
    
    const int iv_size = 12;
    std::vector<unsigned char> iv(iv_size);
    if (RAND_bytes(iv.data(), iv_size) != 1) {
        handleOpenSSLError("RAND_bytes");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        handleOpenSSLError("EVP_EncryptInit_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_size, nullptr) != 1) {
        handleOpenSSLError("EVP_CTRL_GCM_SET_IVLEN");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        handleOpenSSLError("EVP_EncryptInit_ex key/iv");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    output.clear();
    output.insert(output.end(), iv.begin(), iv.end());
    
    std::vector<unsigned char> temp(input.size() + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    int ciphertext_len = 0;
    
    if (EVP_EncryptUpdate(ctx, temp.data(), &len, input.data(), input.size()) != 1) {
        handleOpenSSLError("EVP_EncryptUpdate");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, temp.data() + len, &len) != 1) {
        handleOpenSSLError("EVP_EncryptFinal_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    ciphertext_len += len;
    
    output.insert(output.end(), temp.begin(), temp.begin() + ciphertext_len);
    
    std::vector<unsigned char> tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        handleOpenSSLError("EVP_CTRL_GCM_GET_TAG");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    output.insert(output.end(), tag.begin(), tag.end());
    
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

bool CryptoUtils::aes256GcmDecrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    const int iv_size = 12;
    const int tag_size = 16;
    
    if (input.size() < (size_t)(iv_size + tag_size)) {
        std::cerr << "Input too short for GCM" << std::endl;
        return false;
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        handleOpenSSLError("aes256GcmDecrypt");
        return false;
    }
    
    const unsigned char* iv = input.data();
    const unsigned char* ciphertext = input.data() + iv_size;
    size_t ciphertext_len = input.size() - iv_size - tag_size;
    const unsigned char* tag = input.data() + input.size() - tag_size;
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        handleOpenSSLError("EVP_DecryptInit_ex");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_size, nullptr) != 1) {
        handleOpenSSLError("EVP_CTRL_GCM_SET_IVLEN");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) != 1) {
        handleOpenSSLError("EVP_DecryptInit_ex key/iv");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    output.clear();
    std::vector<unsigned char> temp(ciphertext_len + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    int plaintext_len = 0;
    
    if (EVP_DecryptUpdate(ctx, temp.data(), &len, ciphertext, ciphertext_len) != 1) {
        handleOpenSSLError("EVP_DecryptUpdate");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len = len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag_size, (void*)tag) != 1) {
        handleOpenSSLError("EVP_CTRL_GCM_SET_TAG");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_DecryptFinal_ex(ctx, temp.data() + len, &len) != 1) {
        handleOpenSSLError("EVP_DecryptFinal_ex - authentication failed");
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len += len;
    
    output.assign(temp.begin(), temp.begin() + plaintext_len);
    
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

bool CryptoUtils::aes256EcbEncrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_aes_256_ecb(), input, output, key, 0, true, false);
}

bool CryptoUtils::aes256EcbDecrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_aes_256_ecb(), input, output, key, 0, true, false);
}

bool CryptoUtils::aes256CbcEncrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_aes_256_cbc(), input, output, key, 16, true, true);
}

bool CryptoUtils::aes256CbcDecrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_aes_256_cbc(), input, output, key, 16, true, true);
}

bool CryptoUtils::aes256CfbEncrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_aes_256_cfb8(), input, output, key, 16, false, true);
}

bool CryptoUtils::aes256CfbDecrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_aes_256_cfb8(), input, output, key, 16, false, true);
}

bool CryptoUtils::aes256OfbEncrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_aes_256_ofb(), input, output, key, 16, false, true);
}

bool CryptoUtils::aes256OfbDecrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_aes_256_ofb(), input, output, key, 16, false, true);
}

bool CryptoUtils::aes256CtrEncrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_aes_256_ctr(), input, output, key, 16, false, true);
}

bool CryptoUtils::aes256CtrDecrypt(const std::vector<unsigned char>& input,
                                   std::vector<unsigned char>& output,
                                   const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_aes_256_ctr(), input, output, key, 16, false, true);
}

bool CryptoUtils::sm4EcbEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_sm4_ecb(), input, output, key, 0, true, false);
}

bool CryptoUtils::sm4EcbDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_sm4_ecb(), input, output, key, 0, true, false);
}

bool CryptoUtils::sm4CbcEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_sm4_cbc(), input, output, key, 16, true, true);
}

bool CryptoUtils::sm4CbcDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_sm4_cbc(), input, output, key, 16, true, true);
}

bool CryptoUtils::sm4CtrEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key) {
    return genericEncrypt(EVP_sm4_ctr(), input, output, key, 16, false, true);
}

bool CryptoUtils::sm4CtrDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key) {
    return genericDecrypt(EVP_sm4_ctr(), input, output, key, 16, false, true);
}

bool CryptoUtils::rsaOaepEncrypt(const std::vector<unsigned char>& input,
                                 std::vector<unsigned char>& output,
                                 const std::vector<unsigned char>& key) {
    std::cerr << "RSA-OAEP encryption not fully implemented" << std::endl;
    (void)input;
    (void)output;
    (void)key;
    return false;
}

bool CryptoUtils::rsaOaepDecrypt(const std::vector<unsigned char>& input,
                                 std::vector<unsigned char>& output,
                                 const std::vector<unsigned char>& key) {
    std::cerr << "RSA-OAEP decryption not fully implemented" << std::endl;
    (void)input;
    (void)output;
    (void)key;
    return false;
}

bool CryptoUtils::xorEncrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key) {
    if (key.empty()) return false;
    
    output.resize(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        output[i] = input[i] ^ key[i % key.size()];
    }
    return true;
}

bool CryptoUtils::xorDecrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key) {
    return xorEncrypt(input, output, key);
}

bool CryptoUtils::encryptData(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              EncryptionAlgorithm algorithm,
                              const std::vector<unsigned char>& key) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
            return aes256GcmEncrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_ECB:
            return aes256EcbEncrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_CBC:
            return aes256CbcEncrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_CFB:
            return aes256CfbEncrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_OFB:
            return aes256OfbEncrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_CTR:
            return aes256CtrEncrypt(input, output, key);
        case EncryptionAlgorithm::SM4_ECB:
            return sm4EcbEncrypt(input, output, key);
        case EncryptionAlgorithm::SM4_CBC:
            return sm4CbcEncrypt(input, output, key);
        case EncryptionAlgorithm::SM4_CTR:
            return sm4CtrEncrypt(input, output, key);
        case EncryptionAlgorithm::RSA_OAEP:
            return rsaOaepEncrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_FPGA:
            std::cerr << "[DEBUG] AES_256_FPGA encrypt called, checking FPGA availability..." << std::endl;
            if (FpgaAes::isAvailable()) {
                std::cerr << "[DEBUG] FPGA is available, attempting FPGA encryption..." << std::endl;
                if (FpgaAes::encrypt(input, output, key)) {
                    std::cerr << "[DEBUG] FPGA encryption SUCCESS! Output size: " << output.size() << " bytes" << std::endl;
                    return true;
                }
                std::cerr << "[WARNING] FPGA encryption FAILED, falling back to CPU" << std::endl;
            } else {
                std::cerr << "[DEBUG] FPGA is NOT available, using CPU encryption" << std::endl;
            }
            return aes256EcbEncrypt(input, output, key);
        default:
            return xorEncrypt(input, output, key);
    }
}

bool CryptoUtils::decryptData(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              EncryptionAlgorithm algorithm,
                              const std::vector<unsigned char>& key) {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_256_GCM:
            return aes256GcmDecrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_ECB:
            return aes256EcbDecrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_CBC:
            return aes256CbcDecrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_CFB:
            return aes256CfbDecrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_OFB:
            return aes256OfbDecrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_CTR:
            return aes256CtrDecrypt(input, output, key);
        case EncryptionAlgorithm::SM4_ECB:
            return sm4EcbDecrypt(input, output, key);
        case EncryptionAlgorithm::SM4_CBC:
            return sm4CbcDecrypt(input, output, key);
        case EncryptionAlgorithm::SM4_CTR:
            return sm4CtrDecrypt(input, output, key);
        case EncryptionAlgorithm::RSA_OAEP:
            return rsaOaepDecrypt(input, output, key);
        case EncryptionAlgorithm::AES_256_FPGA:
            std::cerr << "[DEBUG] AES_256_FPGA decrypt called, checking FPGA availability..." << std::endl;
            if (FpgaAes::isAvailable()) {
                std::cerr << "[DEBUG] FPGA is available, attempting FPGA decryption..." << std::endl;
                if (FpgaAes::decrypt(input, output, key)) {
                    std::cerr << "[DEBUG] FPGA decryption SUCCESS! Output size: " << output.size() << " bytes" << std::endl;
                    return true;
                }
                std::cerr << "[WARNING] FPGA decryption FAILED, falling back to CPU" << std::endl;
            } else {
                std::cerr << "[DEBUG] FPGA is NOT available, using CPU decryption" << std::endl;
            }
            return aes256EcbDecrypt(input, output, key);
        default:
            return xorDecrypt(input, output, key);
    }
}

bool CryptoUtils::encryptFile(const std::string& input_file,
                              const std::string& output_file,
                              EncryptionAlgorithm algorithm,
                              const std::vector<unsigned char>& key) {
    std::ifstream ifs(input_file, std::ios::binary);
    if (!ifs) {
        std::cerr << "Cannot open input file: " << input_file << std::endl;
        return false;
    }
    
    std::vector<unsigned char> input((std::istreambuf_iterator<char>(ifs)),
                                     std::istreambuf_iterator<char>());
    ifs.close();
    
    std::vector<unsigned char> output;
    if (!encryptData(input, output, algorithm, key)) {
        return false;
    }
    
    std::ofstream ofs(output_file, std::ios::binary);
    if (!ofs) {
        std::cerr << "Cannot open output file: " << output_file << std::endl;
        return false;
    }
    
    ofs.write(reinterpret_cast<const char*>(output.data()), output.size());
    ofs.close();
    
    return true;
}

bool CryptoUtils::decryptFile(const std::string& input_file,
                              const std::string& output_file,
                              EncryptionAlgorithm algorithm,
                              const std::vector<unsigned char>& key) {
    std::ifstream ifs(input_file, std::ios::binary);
    if (!ifs) {
        std::cerr << "Cannot open input file: " << input_file << std::endl;
        return false;
    }
    
    std::vector<unsigned char> input((std::istreambuf_iterator<char>(ifs)),
                                     std::istreambuf_iterator<char>());
    ifs.close();
    
    std::vector<unsigned char> output;
    if (!decryptData(input, output, algorithm, key)) {
        return false;
    }
    
    std::ofstream ofs(output_file, std::ios::binary);
    if (!ofs) {
        std::cerr << "Cannot open output file: " << output_file << std::endl;
        return false;
    }
    
    ofs.write(reinterpret_cast<const char*>(output.data()), output.size());
    ofs.close();
    
    return true;
}

} // namespace crypto
} // namespace dfs
