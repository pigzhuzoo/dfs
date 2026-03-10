#ifndef CRYPTO_UTILS_HPP
#define CRYPTO_UTILS_HPP

#include <string>
#include <vector>
#include <memory>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rand.h>

namespace dfs {
namespace crypto {

enum class EncryptionAlgorithm {
    AES_256_GCM,    // AES-256-GCM (推荐，认证加密)
    AES_256_ECB,    // AES-256-ECB (不推荐，相同明文产生相同密文)
    AES_256_CBC,    // AES-256-CBC (需要填充)
    AES_256_CFB,    // AES-256-CFB (流密码模式)
    AES_256_OFB,    // AES-256-OFB (流密码模式)
    AES_256_CTR,    // AES-256-CTR (流密码模式，推荐)
    SM4_ECB,        // SM4-ECB
    SM4_CBC,        // SM4-CBC
    SM4_CTR,        // SM4-CTR (推荐)
    RSA_OAEP,       // RSA-OAEP (仅用于小数据加密)
    AES_256_FPGA    // AES-256-ECB on FPGA (高性能硬件加速)
};

class CryptoContext {
public:
    CryptoContext(EncryptionAlgorithm algo, const std::vector<unsigned char>& key);
    ~CryptoContext();
    
    CryptoContext(const CryptoContext&) = delete;
    CryptoContext& operator=(const CryptoContext&) = delete;
    
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

class CryptoUtils {
public:
    static bool encryptFile(const std::string& input_file, 
                           const std::string& output_file,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    static bool decryptFile(const std::string& input_file,
                           const std::string& output_file,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    static bool encryptData(const std::vector<unsigned char>& input,
                           std::vector<unsigned char>& output,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    static bool decryptData(const std::vector<unsigned char>& input,
                           std::vector<unsigned char>& output,
                           EncryptionAlgorithm algorithm,
                           const std::vector<unsigned char>& key);
    
    static std::vector<unsigned char> generateKeyFromPassword(const std::string& password,
                                                            EncryptionAlgorithm algorithm);
    
    static std::string getAlgorithmName(EncryptionAlgorithm algorithm);
    
    static bool isAlgorithmSupported(EncryptionAlgorithm algorithm);
    
    static int getKeySize(EncryptionAlgorithm algorithm);
    
    static int getIVSize(EncryptionAlgorithm algorithm);

private:
    static bool aes256GcmEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256GcmDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    static bool aes256EcbEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256EcbDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    static bool aes256CbcEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256CbcDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    static bool aes256CfbEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256CfbDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    static bool aes256OfbEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256OfbDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    static bool aes256CtrEncrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    static bool aes256CtrDecrypt(const std::vector<unsigned char>& input,
                                std::vector<unsigned char>& output,
                                const std::vector<unsigned char>& key);
    
    static bool sm4EcbEncrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    static bool sm4EcbDecrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    
    static bool sm4CbcEncrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    static bool sm4CbcDecrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    
    static bool sm4CtrEncrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    static bool sm4CtrDecrypt(const std::vector<unsigned char>& input,
                             std::vector<unsigned char>& output,
                             const std::vector<unsigned char>& key);
    
    static bool rsaOaepEncrypt(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key);
    static bool rsaOaepDecrypt(const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key);
    
    static bool xorEncrypt(const std::vector<unsigned char>& input,
                          std::vector<unsigned char>& output,
                          const std::vector<unsigned char>& key);
    static bool xorDecrypt(const std::vector<unsigned char>& input,
                          std::vector<unsigned char>& output,
                          const std::vector<unsigned char>& key);
    
    static void handleOpenSSLError(const char* operation);
    static std::vector<unsigned char> sha256Hash(const std::vector<unsigned char>& data);
    
    static bool genericEncrypt(const EVP_CIPHER* cipher,
                              const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key,
                              int iv_size,
                              bool needs_padding,
                              bool needs_iv);
    
    static bool genericDecrypt(const EVP_CIPHER* cipher,
                              const std::vector<unsigned char>& input,
                              std::vector<unsigned char>& output,
                              const std::vector<unsigned char>& key,
                              int iv_size,
                              bool needs_padding,
                              bool needs_iv);
};

} // namespace crypto
} // namespace dfs

#endif // CRYPTO_UTILS_HPP
