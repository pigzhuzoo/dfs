# Distributed File System - C++ Implementation

[![English](https://img.shields.io/badge/lang-English-blue)](#) [![中文](https://img.shields.io/badge/语言-中文-red)](README_zh.md)

## Project Overview

This is a C++ rewrite of the original C language distributed file system, utilizing modern C++ features and object-oriented design.

## Key Improvements

### 1. Language and Standard Upgrade
- Upgraded from C to C++17 standard
- Utilizes modern C++ features such as smart pointers and STL containers
- Better type safety and memory management

### 2. Object-Oriented Design
- Restructured original structs into classes and structs
- Uses namespaces to organize code
- Implements better encapsulation and abstraction

### 3. Memory Management Optimization
- Uses `std::unique_ptr` for automatic memory management
- Avoids manual memory allocation and deallocation
- Reduces memory leak risks

### 4. Code Structure Improvement
- Header files use `.hpp` extension
- Source files use `.cpp` extension
- Clearer directory structure:
  ```
  dfs_cpp/
  ├── include/     # Header files
  ├── src/         # Source files
  ├── conf/        # Configuration files
  ├── logs/        # Log files
  └── bin/         # Executable files
  ```

### 5. Standard Library Integration
- Uses `std::string` instead of C-style strings
- Uses `std::vector` instead of dynamic arrays
- Uses `std::array` instead of fixed arrays
- Uses `std::ifstream`/`std::ofstream` instead of FILE*

### 6. Error Handling Improvement
- Better exception handling mechanisms
- Type-safe error checking
- Clear error message output

### 7. Multi-Encryption Algorithm Support (New)
- **AES-256-GCM**: AES-256-GCM encryption using OpenSSL EVP interface (recommended, default)
- **AES-256-ECB**: AES-256 encryption using ECB mode (PKCS7 padding fixed)
- **SM4-CTR**: SM4 algorithm CTR mode encryption (if OpenSSL supports it)
- **RSA-OAEP**: RSA-OAEP encryption (mainly for small data encryption)

### 8. Performance Testing Framework (In Development)
- **Basic Performance Testing**: Implemented throughput, latency, and success rate metrics
- **Academic-grade Testing**: Under development with multi-file types, statistical analysis, and resource monitoring
- **Automated Testing**: Integrated into Makefile for one-click execution

## Encryption Support

The distributed file system supports multiple encryption algorithms through OpenSSL's EVP interface:

- **AES-256-GCM (1)**: AES-256 authenticated encryption using GCM mode (recommended, default)
- **AES-256-ECB (2)**: AES-256 encryption using ECB mode (**PKCS7 padding issue fixed**)
- **SM4-CTR (3)**: SM4 cipher in CTR mode (if OpenSSL supports SM4)
- **RSA-OAEP (4)**: RSA encryption with OAEP padding (only suitable for small data)

Configure encryption type in `conf/dfc.conf`:
```conf
EncryptionType: AES_256_GCM

**Note**: ECB mode security considerations
- ECB (Electronic Codebook) is the most basic block cipher mode
- Identical plaintext blocks always produce identical ciphertext blocks, potentially leaking data patterns
- Does not provide data integrity protection
- Suitable for encrypting random data or scenarios requiring fast encryption/decryption
- For structured data, it's recommended to use more secure modes like GCM

**ECB Mode Important Update**: 
- Fixed the issue where ECB mode required input size to be multiple of 16 bytes
- Enabled OpenSSL automatic PKCS7 padding for industrial-grade compatibility
- Supports correct encryption/decryption of files of any size
```

## Core Components

### Utility Class (utils.hpp/cpp)
- File and directory operations
- String processing functions
- **Multi-algorithm encryption/decryption functionality**
- Hash computation

### Network Utility Class (netutils.hpp/cpp)
- Socket communication wrapper
- Data serialization/deserialization
- Network protocol handling

### DFS Utility Class (dfsutils.hpp/cpp)
- Server-side functionality implementation
- User authentication
- Command parsing and execution

### DFC Utility Class (dfcutils.hpp/cpp)
- Client-side functionality implementation
- Connection management
- Command construction and sending
- **Encryption type configuration support**

### Crypto Utility Class (crypto_utils.hpp/cpp) - New
- **Unified encryption interface**
- **OpenSSL EVP interface wrapper**
- **Multiple encryption algorithm support**
- **Automatic key generation**
- **ECB mode PKCS7 padding support**

## Compilation and Execution

### Compilation:
```bash
make all      # Clean and compile all components
make dfs      # Compile server
make dfc      # Compile client
```

### Execution:
```bash
make start    # Start four server instances
make client   # Start client
make run      # View server logs
```

### Performance Testing:
```bash
make perf-test            # Standard performance test
make perf-test-quick      # Quick test (small files)
make perf-test-full       # Full test (large files)
make perf-test-academic   # Academic research standard test (in development)
make perf-plot-only       # Generate plots only
```

> **Note**: Performance testing functionality is under development. Basic throughput, latency, and success rate testing is currently supported. Academic-grade testing features (multi-file types, statistical analysis, resource monitoring, etc.) are being developed.

### Cleanup:
```bash
make clean    # Clean compilation artifacts
make kill     # Terminate server processes
make clear    # Clear DFS directories
```

## Configuration Files

### Client Configuration (conf/dfc.conf)
```conf
Server DFS1 127.0.0.1:10001
Server DFS2 127.0.0.1:10002
Server DFS3 127.0.0.1:10003
Server DFS4 127.0.0.1:10004

Username: Bob
Password: ComplextPassword
# EncryptionType options:
# 1 or AES_256_GCM - AES-256-GCM encryption (recommended, default)
# 2 or AES_256_ECB - AES-256-ECB encryption (with PKCS7 padding)
# 3 or SM4_CTR - SM4-CTR encryption (if OpenSSL supports SM4)
# 4 or RSA_OAEP - RSA-OAEP encryption (for small data only)
EncryptionType: AES_256_GCM
```

## Features

Maintains all features of the original C version and adds:
- **Multi-encryption algorithm support**
- **Secure encryption based on OpenSSL EVP interface**
- **Configurable encryption types**
- **Backward compatible XOR encryption**
- **ECB mode PKCS7 padding support**
- **Performance testing framework (in development)**
- File sharding storage and retrieval
- Multi-server fault tolerance mechanism
- User authentication and permission management
- Support for GET/PUT/LIST/MKDIR commands
- Concurrent client connection handling

## Client Usage Guide

After starting the client, you can enter the following commands in the interactive command line:

### 1. MKDIR - Create Directory
```bash
MKDIR <directory_name>
```
**Example:**
```
>>> MKDIR myfolder
```
Creates the specified directory on all DFS servers.

### 2. LIST - List Files/Directories
```bash
LIST <path>
```
**Example:**
```
>>> LIST /
>>> LIST /myfolder
```
Lists all files and directories in the specified path.

### 3. PUT - Upload File
```bash
PUT <local_file_path> <remote_filename>
```
**Example:**
```
>>> PUT /home/user/document.txt backup.txt
```
Uploads local file to DFS system with optional remote filename. File will be encrypted according to configured encryption type.

### 4. GET - Download File
```bash
GET <remote_filename> <local_save_path>
```
**Example:**
```
>>> GET backup.txt /home/user/restored_document.txt
```
Downloads file from DFS system and decrypts using configured encryption type.

### 5. EXIT/QUIT - Exit Client
```
EXIT
```
```
QUIT
```

## Server Configuration

```
# Server user configuration
# Format: username=password
Bob=ComplextPassword
Alice=SimplePassword123
```

## Dependencies

- **Compiler**: clang++ (C++17 support required)
- **Libraries**: OpenSSL development libraries

```
# Install OpenSSL development libraries
sudo apt-get update
sudo apt-get install libssl-dev
```

## Troubleshooting

### 1. Unable to Connect to Server
```
Unable to Connect to any server
```

**Possible Causes**:
- Server not started
- Port occupied
- Firewall blocking connection

**Solutions**:
```
# Check if server is running
ps aux | grep dfs

# Terminate old processes and restart
make kill
make start

# Check port usage
netstat -tlnp | grep 1000
```

### 2. File Upload/Download Failure
```
File not found
```

**Possible Causes**:
- Filename mismatch
- Incomplete server storage
- Network connection interrupted

**Solutions**:
```
# Check server data directories
ls -la server/DFS*/Bob/

# Re-upload file
# Ensure correct filename is used
```

### 3. Compilation Errors
```
OpenSSL header files not found
```

**Solutions**:
```
# Install OpenSSL development library
sudo apt-get install libssl-dev

# Recompile
make clean
make all
```

### 4. ECB Mode Encryption Issues
```
Input size must be multiple of block size
```

**Solution**:
- Ensure using latest version (PKCS7 padding issue fixed)
- If issues persist, check OpenSSL version compatibility

## Technical Details

### File Sharding Algorithm
File sharding is based on SHA256 hash of file content:
```
mod = hash(file_content) % 4
```
Shard distribution strategy determined by mod value:
- mod=0: Shard1→Server1,2; Shard2→Server2,3; Shard3→Server3,4; Shard4→Server4,1
- mod=1: Shard1→Server2,3; Shard2→Server3,4; Shard3→Server4,1; Shard4→Server1,2
- mod=2: Shard1→Server3,4; Shard2→Server4,1; Shard3→Server1,2; Shard4→Server2,3
- mod=3: Shard1→Server4,1; Shard2→Server1,2; Shard3→Server2,3; Shard4→Server3,4

### Network Protocol
- **Data Format**: Big-endian (network byte order)
- **Integer Transmission**: Use htonl/ntohl conversion
- **String Transmission**: Null-terminated C strings
- **Error Handling**: Return -1 for error, 0 for success

### Encryption Mechanism
- **Algorithms**: Support multiple modern encryption algorithms
- **Key**: SHA256 hash of user password
- **Padding**: ECB mode uses automatic PKCS7 padding
- **Security**: AES-256-GCM mode recommended

## Extension and Customization

### Adding New Servers
1. Modify `conf/dfc.conf` to add new Server lines
2. Add user configuration in `conf/dfs.conf`
3. Update sharding algorithm logic (requires source code modification)

### Changing Ports
1. Modify port numbers in `conf/dfc.conf`
2. Start server with new port: `bin/dfs server/DFS1 20001`

### Custom Encryption
Modify encryption functions in `src/crypto_utils.cpp`:
- `encryptData()`: Unified encryption interface
- `decryptData()`: Unified decryption interface
- Support switching between multiple encryption algorithms

## Contribution Guidelines

Pull Requests and Issues are welcome! Please follow these guidelines:

1. **Code Style**: Use clang-format to format code
2. **Testing**: Ensure all existing tests pass
3. **Documentation**: Update relevant documentation and comments
4. **Commit Messages**: Use clear commit messages describing changes

## License

This project is for learning and research purposes only.

## Contact

For questions or suggestions, please contact the project maintainer.

---

**Note**: This system is suitable for learning distributed system principles and is not recommended for production environments. For production-grade distributed file systems, consider mature solutions like HDFS, Ceph, or GlusterFS.

**Current Development Status**: 
- ✅ **Encryption Features**: Multi-algorithm support completed, ECB mode padding issue fixed
- 🚧 **Performance Testing**: Basic framework completed, academic-grade features under development
- 🔜 **Future Plans**: Complete performance testing with statistical analysis, multi-file type support, and resource monitoring