# Distributed File System (DFS) - C++ Implementation

[![English](https://img.shields.io/badge/lang-English-blue)](#) [![中文](https://img.shields.io/badge/语言-中文-red)](README_zh.md)

## Project Overview

This is a C++ rewrite of the [https://github.com/Hasil-Sharma/distributed-file-system](https://github.com/Hasil-Sharma/distributed-file-system) project. The original project was implemented in C language, and this project has been refactored using C++17 standard while maintaining the original core functionality and architectural design.

## Core Features

### 📁 File Sharding and Redundant Storage
- **Intelligent Sharding Algorithm**: Files are split into 4 logical blocks based on hash value modulo operation of file content
- **Redundant Storage**: Each block is stored on 2 different servers, providing fault tolerance capability
- **Integrity Verification**: Automatically detects file integrity to ensure data consistency

### 🔒 Security and Authentication
- **User Authentication**: Supports multi-user login with independent storage space for each user
- **Data Encryption**: Uses XOR simple encryption algorithm to protect transmitted data
- **Access Isolation**: Complete isolation between users' data, no interference

### ⚡ High Performance Design
- **Concurrent Processing**: Server supports multiple client concurrent connections
- **Efficient Transmission**: Optimized network protocol reduces transmission overhead
- **Memory Management**: Uses smart pointers for automatic memory management, avoiding leaks

### 🛠️ Complete Command Set
- **MKDIR**: Create directory
- **LIST**: List files and directories
- **PUT**: Upload file to distributed storage
- **GET**: Download file from distributed storage
- **EXIT/QUIT**: Gracefully exit client

## System Architecture

```
+------------------+     +------------------+
|   Client (DFC)   |     |   Server (DFS)   |
+------------------+     +------------------+
|                  |     |                  |
|  • Connection    |<--->|  • Socket        |
|    Management    | TCP |    Listening     |
|  • Command       |     |  • Command       |
|    Parsing       |     |    Processing    |
|  • File Sharding |     |  • User          |
|  • Data          |     |    Authentication|
|    Encryption    |     |  • File          |
|                  |     |    Operations    |
+------------------+     +------------------+
         ↑                        ↑
         |                        |
+------------------+     +------------------+
| Configuration    |     | Data Storage     |
| • dfc.conf       |     | • server/DFS1/   |
| • User           |     | • server/DFS2/   |
|   Credentials    |     | • server/DFS3/   |
|                  |     | • server/DFS4/   |
+------------------+     +------------------+
```

## Project Structure

```
dfs_cpp/
├── server/               # Server data storage directory
│   ├── DFS1/             # Server 1 data directory
│   ├── DFS2/             # Server 2 data directory  
│   ├── DFS3/             # Server 3 data directory
│   └── DFS4/             # Server 4 data directory
├── include/              # Header files directory
│   ├── debug.hpp         # Debug utility class
│   ├── utils.hpp         # General utility class (file operations, encryption, string processing)
│   ├── netutils.hpp      # Network utility class (Socket communication, data serialization)
│   ├── dfsutils.hpp      # DFS server utility class (command processing, user management)
│   └── dfcutils.hpp      # DFC client utility class (connection management, command building)
├── src/                  # Source files directory
│   ├── utils.cpp         # General utility implementation
│   ├── netutils.cpp      # Network utility implementation  
│   ├── dfsutils.cpp      # DFS server implementation
│   ├── dfcutils.cpp      # DFC client implementation
│   ├── dfs.cpp           # DFS server main program
│   └── dfc.cpp           # DFC client main program
├── conf/                 # Configuration files directory
│   ├── dfc.conf          # Client configuration file
│   └── dfs.conf          # Server configuration file
├── logs/                 # Log files directory
│   ├── dfs1.log          # Server 1 log
│   ├── dfs2.log          # Server 2 log
│   ├── dfs3.log          # Server 3 log
│   ├── dfs4.log          # Server 4 log
├── tests/                # Test scripts directory
│   ├── test_commands.sh  # Command test script
│   ├── test_get.sh       # GET command test script
│   ├── test_put.py       # PUT command test script
│   ├── test_client.py    # Client test script
│   └── *.txt             # Temporary test files
├── bin/                  # Executable files directory
│   ├── dfs               # DFS server executable
│   └── dfc               # DFC client executable
├── obj/                  # Compilation intermediate files directory
├── Makefile              # Build script
└── README.md             # Project documentation (English)
```

## Quick Start

### Environment Requirements
- **Operating System**: Linux (Ubuntu 20.04 LTS recommended)
- **Compiler**: clang++ (C++17 support required)
- **Dependencies**: OpenSSL development libraries
- **Build Tools**: GNU Make

### Install Dependencies
```bash
# Install OpenSSL development libraries
sudo apt-get update
sudo apt-get install libssl-dev
```

### Build Project
```bash
# Build all components (clean and rebuild)
make all

# Or build separately
make dfs    # Build server
make dfc    # Build client
```

### Start System
```bash
# Start 4 server instances (ports 10001-10004)
make start

# Start client
make client
```

### Clean Environment
```bash
# Terminate all server processes
make kill

# Clear server data directories
make clear

# Clean compilation artifacts
make clean
```

## Client Usage Guide

After starting the client, you will see the `>>>` prompt where you can enter the following commands:

### 1. MKDIR - Create Directory
**Syntax**: `MKDIR <directory_name>`

**Function**: Creates the specified directory on all DFS servers

**Examples**:
```
>>> MKDIR documents
>>> MKDIR projects/
```

**Notes**:
- Directory names can include or exclude trailing slash
- Directory will be created under user directory on all servers
- Error message displayed if directory already exists

### 2. LIST - List Files/Directories
**Syntax**: `LIST <path>`

**Function**: Lists all files and directories in the specified path

**Examples**:
```
>>> LIST /
>>> LIST /documents
>>> LIST /projects/
```

**Output Format**:
- Filenames displayed directly
- Directory names displayed with `/` suffix
- Incomplete files marked as `[INCOMPLETE]`

**Notes**:
- Path must start with `/` for absolute path
- Empty path or `/` represents root directory
- Automatic deduplication to avoid duplicate display

### 3. PUT - Upload File
**Syntax**: `PUT <local_file_path> <remote_filename>`

**Function**: Uploads local file to DFS system

**Examples**:
```
>>> PUT /home/user/report.pdf backup_report.pdf
>>> PUT ./config.txt config_backup.txt
```

**Workflow**:
1. Read local file content
2. Calculate file content hash value to determine sharding strategy
3. Split file into 4 logical blocks
4. Encrypt each block and send to corresponding 2 servers
5. Display upload success message

**Notes**:
- Local file path can be absolute or relative
- Remote filename cannot contain path separators
- Files are stored as hidden files (starting with `.`)

### 4. GET - Download File
**Syntax**: `GET <remote_filename> <local_save_path>`

**Function**: Downloads file from DFS system to local

**Examples**:
```
>>> GET backup_report.pdf /home/user/restored.pdf
>>> GET config_backup.txt ./restored_config.txt
```

**Workflow**:
1. Query all servers to get file shard information
2. Verify file integrity (check if all shards exist)
3. Download required shards from corresponding servers
4. Decrypt and reassemble file
5. Save to specified local path

**Notes**:
- Remote filename must exactly match the name specified during PUT
- Local save path directory must exist
- Error message displayed if file is incomplete

### 5. Exit Client
**Syntax**: `EXIT` or `QUIT`

**Function**: Gracefully exits client program

**Examples**:
```
>>> EXIT
<<< Goodbye!

>>> QUIT  
<<< Goodbye!
```

**Notes**:
- Command is case-insensitive (exit/EXIT, quit/QUIT both work)
- Automatically closes all server connections
- Returns to shell prompt after normal exit

## Configuration Files

### Client Configuration (conf/dfc.conf)
```ini
Server DFS1 127.0.0.1:10001
Server DFS2 127.0.0.1:10002  
Server DFS3 127.0.0.1:10003
Server DFS4 127.0.0.1:10004

Username: Bob
Password: ComplextPassword
```

**Configuration Items**:
- `Server <name> <IP:port>`: Defines server nodes
- `Username`: Default login username
- `Password`: Default login password

### Server Configuration (conf/dfs.conf)
```
# Server user configuration
# Format: username=password
Bob=ComplextPassword
Alice=SimplePassword123
```

**Configuration Items**:
- Each line defines a user account
- Format is `username=password`
- Password used as file encryption key

## Troubleshooting

### Common Issues and Solutions

#### 1. Unable to Connect to Server
**Error Message**: `Unable to Connect to any server`

**Possible Causes**:
- Server not started
- Port occupied
- Firewall blocking connection

**Solutions**:
```bash
# Check if server is running
ps aux | grep dfs

# Terminate old processes and restart
make kill
make start

# Check port usage
netstat -tlnp | grep 1000
```

#### 2. File Upload/Download Failure
**Error Message**: `File not found` or `Incomplete file`

**Possible Causes**:
- Filename mismatch
- Incomplete server storage
- Network connection interrupted

**Solutions**:
```bash
# Check server data directories
ls -la server/DFS*/Bob/

# Re-upload file
# Ensure correct filename is used
```

#### 3. Compilation Errors
**Error Message**: OpenSSL header files not found

**Solutions**:
```bash
# Install OpenSSL development libraries
sudo apt-get install libssl-dev

# Rebuild
make clean
make all
```

## Technical Details

### File Sharding Algorithm
File sharding is based on SHA256 hash value calculation of file content:
- `mod = hash(file_content) % 4`
- Shard distribution strategy determined by mod value:
  - mod=0: Shard 1→servers 1,2; Shard 2→servers 2,3; Shard 3→servers 3,4; Shard 4→servers 4,1
  - mod=1: Shard 1→servers 2,3; Shard 2→servers 3,4; Shard 3→servers 4,1; Shard 4→servers 1,2
  - mod=2: Shard 1→servers 3,4; Shard 2→servers 4,1; Shard 3→servers 1,2; Shard 4→servers 2,3  
  - mod=3: Shard 1→servers 4,1; Shard 2→servers 1,2; Shard 3→servers 2,3; Shard 4→servers 3,4

### Network Protocol
- **Data Format**: Big-endian (network byte order)
- **Integer Transmission**: Uses htonl/ntohl conversion
- **String Transmission**: Null-terminated C strings
- **Error Handling**: Returns -1 for error, 0 for success

### Encryption Mechanism
- **Algorithm**: XOR simple encryption
- **Key**: SHA256 hash of user password
- **Security**: Suitable for demonstration purposes, production environments should use stronger encryption


## Extension and Customization

### Add New Server
1. Modify `conf/dfc.conf` to add new Server line
2. Add user configuration in `conf/dfs.conf`
3. Update sharding algorithm logic (requires source code modification)

### Change Port
1. Modify port numbers in `conf/dfc.conf`
2. Start server with new port: `bin/dfs server/DFS1 20001`

### Custom Encryption
Modify encryption functions in `src/utils.cpp`:
- `encryptSplit()`: File shard encryption
- `decryptSplit()`: File shard decryption

## Contribution Guidelines

Pull requests and issues are welcome! Please follow these guidelines:

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