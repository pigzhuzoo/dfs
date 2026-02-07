#ifndef CRYPTO_UTILS_HPP
#define CRYPTO_UTILS_HPP

#include <string>
#include <vector>
#include <memory>

// OpenSSL头文件
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rand.h>

namespace dfs {
namespace crypto {

// 加密算法枚举
enum class EncryptionAlgorithm {
    AES_256_GCM,    // AES-256-GCM
    AES_256_ECB,    // AES-256-ECB
    SM4_CTR,        // SM4-CTR (如果OpenSSL支持)
    RSA_OAEP        // RSA-OAEP (主要用于小数据加密)
};

// 加密上下文类
class CryptoContext {
public:
    CryptoContext(EncryptionAlgorithm algo, const std::vector<unsigned char>& key);
    ~CryptoContext();
    
    // 禁用拷贝构造和赋值
    CryptoContext(const CryptoContext&) = delete;
    CryptoContext& operator=(const CryptoContext&) = delete;
    
    // 移动构造和赋值
    CryptoContext(CryptoContext&& other) noexcept;
    CryptoContext& operator=(CryptoContext&& other) noexcept;
    
    bool isValid() const { return valid_; }
    EncryptionAlgorithm getAlgorithm() const { return algorithm_; }
    
private:
    EncryptionAlgorithm algorithm_;
    std::vector<unsigned char> key_;
    EVP_CIPHER_CTX* cipher_ctx_;
    RSA* rsa_key_;
    bool valid_;
    
    friend class CryptoUtils;
};

// 加密工具类
class CryptoUtils {
public:
    // 文件加密
    static bool encryptFile(const std::string& input_file, 
                           const std::string& output_file,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    // 文件解密
    static bool decryptFile(const std::string& input_file,
                           const std::string& output_file,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    // 内存数据加密
    static bool encryptData(const std::vector<unsigned char>& input,
                           std::vector<unsigned char>& output,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    // 内存数据解密
    static bool decryptData(const std::vector<unsigned char>& input,
                           std::vector<unsigned char>& output,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    // 从字符串生成密钥
    static std::vector<unsigned char> generateKeyFromPassword(const std::string& password,
                                                            EncryptionAlgorithm algorithm);
    
    // 获取算法名称
    static std::string getAlgorithmName(EncryptionAlgorithm algorithm);
    
    // 检查算法是否可用
    static bool isAlgorithmSupported(EncryptionAlgorithm algorithm);

private:
    // AES-256-GCM实现
    static bool aes256GcmEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256GcmDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    // AES-256-ECB实现
    static bool aes256EcbEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256EcbDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    // SM4-CTR实现（如果可用）
    static bool sm4CtrEncrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    static bool sm4CtrDecrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    
    // RSA-OAEP实现
    static bool rsaOaepEncrypt(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key);
    static bool rsaOaepDecrypt(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key);
    
    // XOR实现（向后兼容）
    static bool xorEncrypt(const std::vector<unsigned char>& input,
                          std::vector<unsigned char>& output,
                          const std::vector<unsigned char>& key);
    static bool xorDecrypt(const std::vector<unsigned char>& input,
                          std::vector<unsigned char>& output,
                          const std::vector<unsigned char>& key);
    
    // 辅助函数
    static void handleOpenSSLError(const char* operation);
    static std::vector<unsigned char> sha256Hash(const std::vector<unsigned char>& data);
};

} // namespace crypto
} // namespace dfs

#endif // CRYPTO_UTILS_HPP