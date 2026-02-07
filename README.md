# 分布式文件系统 - C++版本

这是原C语言分布式文件系统的C++重写版本，采用了现代化的C++特性和面向对象设计。

## 主要改进

### 1. 语言和标准升级
- 从C语言升级到C++17标准
- 使用现代C++特性如智能指针、STL容器等
- 更好的类型安全和内存管理

### 2. 面向对象设计
- 将原有的结构体重构为类和结构体
- 使用命名空间组织代码
- 实现更好的封装和抽象

### 3. 内存管理优化
- 使用`std::unique_ptr`自动管理内存
- 避免手动内存分配和释放
- 减少内存泄漏风险

### 4. 代码结构改进
- 头文件使用`.hpp`扩展名
- 源文件使用`.cpp`扩展名
- 更清晰的目录结构：
  ```
  dfs_cpp/
  ├── include/     # 头文件
  ├── src/         # 源文件
  ├── conf/        # 配置文件
  ├── logs/        # 日志文件
  └── bin/         # 可执行文件
  ```

### 5. 标准库集成
- 使用`std::string`替代C风格字符串
- 使用`std::vector`替代动态数组
- 使用`std::array`替代固定数组
- 使用`std::ifstream`/`std::ofstream`替代FILE*

### 6. 错误处理改进
- 更好的异常处理机制
- 类型安全的错误检查
- 清晰的错误信息输出

### 7. 多加密算法支持（新增）
- **AES-256-GCM**: 使用OpenSSL EVP接口的AES-256-GCM加密（推荐，默认）
- **AES-256-ECB**: 使用ECB模式的AES-256加密（已修复填充问题）
- **SM4-CTR**: 国密SM4算法CTR模式加密（如果OpenSSL支持）
- **RSA-OAEP**: RSA-OAEP加密（主要用于小数据加密）

### 8. 性能测试框架（开发中）
- **基础性能测试**: 已实现吞吐量、时延、成功率等指标测试
- **学术级测试**: 正在完善多文件类型、统计分析、资源监控等功能
- **自动化测试**: 已集成到Makefile，支持一键运行

## 加密支持

分布式文件系统通过OpenSSL的EVP接口支持多种加密算法：

- **AES-256-GCM (1)**: 使用GCM模式的AES-256认证加密（推荐，默认）
- **AES-256-ECB (2)**: 使用ECB模式的AES-256加密（**已修复PKCS7填充问题**）
- **SM4-CTR (3)**: CTR模式的SM4密码（如果OpenSSL支持SM4）
- **RSA-OAEP (4)**: 使用OAEP填充的RSA加密（仅适用于小数据）

在`conf/dfc.conf`中配置加密类型：
```conf
EncryptionType: AES_256_GCM

**注意**: ECB模式的安全性考虑
- ECB（Electronic Codebook）是最基本的分组密码工作模式
- 相同的明文块总是产生相同的密文块，可能泄露数据模式
- 不提供数据完整性保护
- 适合加密随机数据或需要快速加密/解密的场景
- 对于结构化数据，建议使用GCM等更安全的模式

**ECB模式重要更新**: 
- 已修复ECB模式要求输入大小为16字节倍数的问题
- 启用OpenSSL自动PKCS7填充，确保工业级兼容性
- 支持任意大小文件的正确加密/解密
```

## 核心组件

### 工具类 (utils.hpp/cpp)
- 文件和目录操作
- 字符串处理函数
- **多算法加密解密功能**
- 哈希计算

### 网络工具类 (netutils.hpp/cpp)
- Socket通信封装
- 数据序列化/反序列化
- 网络协议处理

### DFS工具类 (dfsutils.hpp/cpp)
- 服务器端功能实现
- 用户认证
- 命令解析和执行

### DFC工具类 (dfcutils.hpp/cpp)
- 客户端功能实现
- 连接管理
- 命令构建和发送
- **加密类型配置支持**

### 加密工具类 (crypto_utils.hpp/cpp) - 新增
- **统一的加密接口**
- **OpenSSL EVP接口封装**
- **多种加密算法支持**
- **自动密钥生成**
- **ECB模式PKCS7填充支持**

## 编译和运行

### 编译:
```bash
make all      # 清理并编译所有组件
make dfs      # 编译服务器
make dfc      # 编译客户端
```

### 运行:
```bash
make start    # 启动四个服务器实例
make client   # 启动客户端
make run      # 查看服务器日志
```

### 性能测试:
```bash
make perf-test            # 标准性能测试
make perf-test-quick      # 快速测试（小文件）
make perf-test-full       # 完整测试（大文件）
make perf-test-academic   # 学术研究标准测试（开发中）
make perf-plot-only       # 仅生成图表
```

> **注意**: 性能测试功能正在完善中，目前支持基础的吞吐量、时延和成功率测试。学术级测试功能（多文件类型、统计分析、资源监控等）正在开发中。

### 清理:
```bash
make clean    # 清理编译产物
make kill     # 终止服务器进程
make clear    # 清空DFS目录
```

## 配置文件

### 客户端配置 (conf/dfc.conf)
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

## 功能特性

保持了原有C版本的所有功能，并增加了：
- **多加密算法支持**
- **基于OpenSSL EVP接口的安全加密**
- **可配置的加密类型**
- **向后兼容的XOR加密**
- **ECB模式PKCS7填充支持**
- **性能测试框架（开发中）**
- 文件分片存储和检索
- 多服务器容错机制
- 用户认证和权限管理
- 支持GET/PUT/LIST/MKDIR命令
- 并发客户端连接处理

## 客户端使用指南

启动客户端后，您可以在交互式命令行中输入以下命令：

### 1. MKDIR - 创建目录
```bash
MKDIR <目录名>
```
**示例:**
```
>>> MKDIR myfolder
```
在所有DFS服务器上创建指定目录。

### 2. LIST - 列出文件/目录
```bash
LIST <路径>
```
**示例:**
```
>>> LIST /
>>> LIST /myfolder
```
列出指定路径下的所有文件和目录。

### 3. PUT - 上传文件
```bash
PUT <本地文件路径> <远程文件名>
```
**示例:**
```
>>> PUT /home/user/document.txt backup.txt
```
将本地文件上传到DFS系统中，并可指定远程文件名。文件会根据配置的加密类型进行加密。

### 4. GET - 下载文件
```bash
GET <远程文件名> <本地保存路径>
```
**示例:**
```
>>> GET backup.txt /home/user/restored_document.txt
```
从DFS系统下载文件，并使用配置的加密类型进行解密。

### 5. EXIT/QUIT - 退出客户端
```
EXIT
```
```
QUIT
```

## 服务器配置

```
# Server user configuration
# Format: username=password
Bob=ComplextPassword
Alice=SimplePassword123
```

## 依赖

- **编译器**: clang++ (C++17 support required)
- **依赖库**: OpenSSL development libraries

```
# Install OpenSSL development libraries
sudo apt-get update
sudo apt-get install libssl-dev
```

## 问题排查

### 1. 无法连接到服务器
```
Unable to Connect to any server
```

**可能原因**:
- 服务器未启动
- 端口被占用
- 防火墙阻止连接

**解决方法**:
```
# 检查服务器是否运行
ps aux | grep dfs

# 终止旧进程并重启
make kill
make start

# 检查端口使用情况
netstat -tlnp | grep 1000
```

### 2. 文件上传/下载失败
```
File not found
```

**可能原因**:
- 文件名不匹配
- 服务器存储不完整
- 网络连接中断

**解决方法**:
```
# 检查服务器数据目录
ls -la server/DFS*/Bob/

# 重新上传文件
# 确保使用正确的文件名
```

### 3. 编译错误
```
OpenSSL header files not found
```

**解决方法**:
```
# 安装OpenSSL开发库
sudo apt-get install libssl-dev

# 重新编译
make clean
make all
```

### 4. ECB模式加密问题
```
Input size must be multiple of block size
```

**解决方案**:
- 确保使用最新版本（已修复PKCS7填充问题）
- 如果仍有问题，请检查OpenSSL版本兼容性

## 技术细节

### 文件分片算法
文件分片基于文件内容的SHA256哈希值计算：
```
mod = hash(file_content) % 4
```
分片分布策略由mod值决定：
- mod=0: 分片1→服务器1,2; 分片2→服务器2,3; 分片3→服务器3,4; 分片4→服务器4,1
- mod=1: 分片1→服务器2,3; 分片2→服务器3,4; 分片3→服务器4,1; 分片4→服务器1,2
- mod=2: 分片1→服务器3,4; 分片2→服务器4,1; 分片3→服务器1,2; 分片4→服务器2,3
- mod=3: 分片1→服务器4,1; 分片2→服务器1,2; 分片3→服务器2,3; 分片4→服务器3,4

### 网络协议
- **数据格式**: 大端序（网络字节序）
- **整数传输**: 使用htonl/ntohl转换
- **字符串传输**: 以空字符结尾的C字符串
- **错误处理**: 返回-1表示错误，0表示成功

### 加密机制
- **算法**: 支持多种现代加密算法
- **密钥**: 用户密码的SHA256哈希值
- **填充**: ECB模式使用PKCS7自动填充
- **安全性**: 推荐使用AES-256-GCM模式

## 扩展和定制

### 添加新服务器
1. 修改`conf/dfc.conf`添加新的Server行
2. 在`conf/dfs.conf`中添加用户配置
3. 更新分片算法逻辑（需要修改源代码）

### 更改端口
1. 修改`conf/dfc.conf`中的端口号
2. 使用新端口启动服务器: `bin/dfs server/DFS1 20001`

### 自定义加密
修改`src/crypto_utils.cpp`中的加密函数：
- `encryptData()`: 统一加密接口
- `decryptData()`: 统一解密接口
- 支持多种加密算法切换

## 贡献指南

欢迎提交Pull Requests和Issues！请遵循以下指南：

1. **代码风格**: 使用clang-format格式化代码
2. **测试**: 确保所有现有测试通过
3. **文档**: 更新相关文档和注释
4. **提交信息**: 使用清晰的提交信息描述更改

## 许可证

本项目仅供学习和研究用途。

## 联系

如有问题或建议，请联系项目维护者。

---

**注意**: 本系统适合学习分布式系统原理，不推荐用于生产环境。对于生产级分布式文件系统，建议使用成熟的解决方案如HDFS、Ceph或GlusterFS。

**当前开发状态**: 
- ✅ **加密功能**: 已完成多算法支持，ECB模式填充问题已修复
- 🚧 **性能测试**: 基础框架已完成，学术级功能正在完善中
- 🔜 **未来计划**: 完善性能测试的统计分析、多文件类型支持和资源监控