# 分布式文件系统 (Distributed File System) - C++ 实现

[![English](https://img.shields.io/badge/lang-English-blue)](README.md) [![中文](https://img.shields.io/badge/语言-中文-red)](#)

## 项目概述

这是对 [https://github.com/Hasil-Sharma/distributed-file-system](https://github.com/Hasil-Sharma/distributed-file-system) 的 C++ 重写版本。原项目使用C语言实现，本项目采用C++17标准进行重构，保持了原有的核心功能和架构设计。

## 核心特性

### 📁 文件分片与冗余存储
- **智能分片算法**: 基于文件内容哈希值的模运算，将文件分割成4个逻辑块
- **冗余存储**: 每个块存储在2个不同的服务器上，提供容错能力
- **完整性验证**: 自动检测文件完整性，确保数据一致性

### 🔒 安全与认证
- **用户认证**: 支持多用户登录，每个用户有独立的存储空间
- **数据加密**: 使用XOR简单加密算法保护传输数据
- **访问隔离**: 用户间数据完全隔离，互不干扰

### ⚡ 高性能设计
- **并发处理**: 服务器支持多客户端并发连接
- **高效传输**: 优化的网络协议减少传输开销
- **内存管理**: 使用智能指针自动管理内存，避免泄漏

### 🛠️ 完整的命令集
- **MKDIR**: 创建目录
- **LIST**: 列出文件和目录
- **PUT**: 上传文件到分布式存储
- **GET**: 从分布式存储下载文件
- **EXIT/QUIT**: 优雅退出客户端

## 系统架构

```
+------------------+     +------------------+
|   Client (DFC)   |     |   Server (DFS)   |
+------------------+     +------------------+
|                  |     |                  |
|  • 连接管理       |<--->|  • Socket监听    |
|  • 命令解析       | TCP |  • 命令处理      |
|  • 文件分片       |     |  • 用户认证      |
|  • 数据加密       |     |  • 文件操作      |
|                  |     |                  |
+------------------+     +------------------+
         ↑                        ↑
         |                        |
+------------------+     +------------------+
| Configuration    |     | Data Storage     |
| • dfc.conf       |     | • server/DFS1/   |
| • 用户凭证        |     | • server/DFS2/   |
|                  |     | • server/DFS3/   |
|                  |     | • server/DFS4/   |
+------------------+     +------------------+
```

## 项目结构

```
dfs_cpp/
├── server/               # 服务器数据存储目录
│   ├── DFS1/             # 服务器1数据目录
│   ├── DFS2/             # 服务器2数据目录  
│   ├── DFS3/             # 服务器3数据目录
│   └── DFS4/             # 服务器4数据目录
├── include/              # 头文件目录
│   ├── debug.hpp         # 调试工具类
│   ├── utils.hpp         # 通用工具类（文件操作、加密、字符串处理）
│   ├── netutils.hpp      # 网络工具类（Socket通信、数据序列化）
│   ├── dfsutils.hpp      # DFS服务器工具类（命令处理、用户管理）
│   └── dfcutils.hpp      # DFC客户端工具类（连接管理、命令构建）
├── src/                  # 源文件目录
│   ├── utils.cpp         # 通用工具实现
│   ├── netutils.cpp      # 网络工具实现  
│   ├── dfsutils.cpp      # DFS服务器实现
│   ├── dfcutils.cpp      # DFC客户端实现
│   ├── dfs.cpp           # DFS服务器主程序
│   └── dfc.cpp           # DFC客户端主程序
├── conf/                 # 配置文件目录
│   ├── dfc.conf          # 客户端配置文件
│   └── dfs.conf          # 服务器配置文件
├── logs/                 # 日志文件目录
│   ├── dfs1.log          # 服务器1日志
│   ├── dfs2.log          # 服务器2日志
│   ├── dfs3.log          # 服务器3日志
│   ├── dfs4.log          # 服务器4日志
├── tests/                # 测试脚本目录
│   ├── test_commands.sh  # 命令测试脚本
│   ├── test_get.sh       # GET命令测试脚本
│   ├── test_put.py       # PUT命令测试脚本
│   ├── test_client.py    # 客户端测试脚本
│   └── *.txt             # 测试用的临时文件
├── bin/                  # 可执行文件目录
│   ├── dfs               # DFS服务器可执行文件
│   └── dfc               # DFC客户端可执行文件
├── obj/                  # 编译中间文件目录
├── Makefile              # 构建脚本
└── README_zh.md          # 项目说明文档（中文）
```

## 快速开始

### 环境要求
- **操作系统**: Linux (Ubuntu 20.04 LTS 推荐)
- **编译器**: clang++ (支持C++17)
- **依赖库**: OpenSSL 开发库
- **构建工具**: GNU Make

### 安装依赖
```bash
# 安装OpenSSL开发库
sudo apt-get update
sudo apt-get install libssl-dev
```

### 编译项目
```bash
# 编译所有组件（清理并重新编译）
make all

# 或单独编译
make dfs    # 编译服务器
make dfc    # 编译客户端
```

### 启动系统
```bash
# 启动4个服务器实例（端口10001-10004）
make start

# 启动客户端
make client
```

### 清理环境
```bash
# 终止所有服务器进程
make kill

# 清空服务器数据目录
make clear

# 清理编译产物
make clean
```

## 客户端使用指南

启动客户端后，您将看到 `>>>` 提示符，可以输入以下命令：

### 1. MKDIR - 创建目录
**语法**: `MKDIR <目录名>`

**功能**: 在所有DFS服务器上创建指定目录

**示例**:
```
>>> MKDIR documents
>>> MKDIR projects/
```

**注意事项**:
- 目录名可以包含或不包含结尾斜杠
- 目录将在所有服务器的用户目录下创建
- 如果目录已存在，会显示错误信息

### 2. LIST - 列出文件/目录
**语法**: `LIST <路径>`

**功能**: 列出指定路径下的所有文件和目录

**示例**:
```
>>> LIST /
>>> LIST /documents
>>> LIST /projects/
```

**输出格式**:
- 文件名直接显示
- 目录名以 `/` 结尾显示
- 不完整的文件会标记为 `[INCOMPLETE]`

**注意事项**:
- 路径必须以 `/` 开头表示绝对路径
- 空路径或 `/` 表示根目录
- 自动去重，避免重复显示相同内容

### 3. PUT - 上传文件
**语法**: `PUT <本地文件路径> <远程文件名>`

**功能**: 将本地文件上传到DFS系统中

**示例**:
```
>>> PUT /home/user/report.pdf backup_report.pdf
>>> PUT ./config.txt config_backup.txt
```

**工作流程**:
1. 读取本地文件内容
2. 计算文件内容哈希值确定分片策略
3. 将文件分割成4个逻辑块
4. 每个块加密后发送到对应的2个服务器
5. 显示上传成功消息

**注意事项**:
- 本地文件路径可以是绝对路径或相对路径
- 远程文件名不能包含路径分隔符
- 文件会被存储为隐藏文件（以`.`开头）

### 4. GET - 下载文件
**语法**: `GET <远程文件名> <本地保存路径>`

**功能**: 从DFS系统下载文件到本地

**示例**:
```
>>> GET backup_report.pdf /home/user/restored.pdf
>>> GET config_backup.txt ./restored_config.txt
```

**工作流程**:
1. 查询所有服务器获取文件分片信息
2. 验证文件完整性（检查是否所有分片都存在）
3. 从对应服务器下载所需的分片
4. 解密并重组文件
5. 保存到指定本地路径

**注意事项**:
- 远程文件名必须与PUT时指定的名称完全匹配
- 本地保存路径的目录必须存在
- 如果文件不完整，会显示错误信息

### 5. 退出客户端
**语法**: `EXIT` 或 `QUIT`

**功能**: 优雅地退出客户端程序

**示例**:
```
>>> EXIT
<<< Goodbye!

>>> QUIT  
<<< Goodbye!
```

**注意事项**:
- 命令不区分大小写（exit/EXIT, quit/QUIT均可）
- 会自动关闭所有服务器连接
- 正常退出后返回shell提示符

## 配置文件说明

### 客户端配置 (conf/dfc.conf)
```ini
Server DFS1 127.0.0.1:10001
Server DFS2 127.0.0.1:10002  
Server DFS3 127.0.0.1:10003
Server DFS4 127.0.0.1:10004

Username: Bob
Password: ComplextPassword
```

**配置项说明**:
- `Server <名称> <IP:端口>`: 定义服务器节点
- `Username`: 默认登录用户名
- `Password`: 默认登录密码

### 服务器配置 (conf/dfs.conf)
```
# 服务器用户配置
# 格式: username=password
Bob=ComplextPassword
Alice=SimplePassword123
```

**配置项说明**:
- 每行定义一个用户账户
- 格式为 `用户名=密码`
- 密码用于文件加密密钥

## 故障排除

### 常见问题及解决方案

#### 1. 无法连接到服务器
**错误信息**: `Unable to Connect to any server`

**可能原因**:
- 服务器未启动
- 端口被占用
- 防火墙阻止连接

**解决方案**:
```bash
# 检查服务器是否运行
ps aux | grep dfs

# 终止旧进程并重启
make kill
make start

# 检查端口占用
netstat -tlnp | grep 1000
```

#### 2. 文件上传/下载失败
**错误信息**: `File not found` 或 `Incomplete file`

**可能原因**:
- 文件名不匹配
- 服务器存储不完整
- 网络连接中断

**解决方案**:
```bash
# 检查服务器数据目录
ls -la server/DFS*/Bob/

# 重新上传文件
# 确保使用正确的文件名
```

#### 3. 编译错误
**错误信息**: 找不到OpenSSL头文件

**解决方案**:
```bash
# 安装OpenSSL开发库
sudo apt-get install libssl-dev

# 重新编译
make clean
make all
```

## 技术细节

### 文件分片算法
文件分片基于文件内容的SHA256哈希值计算：
- `mod = hash(file_content) % 4`
- 根据mod值确定分片分布策略：
  - mod=0: 分片1→服务器1,2；分片2→服务器2,3；分片3→服务器3,4；分片4→服务器4,1
  - mod=1: 分片1→服务器2,3；分片2→服务器3,4；分片3→服务器4,1；分片4→服务器1,2
  - mod=2: 分片1→服务器3,4；分片2→服务器4,1；分片3→服务器1,2；分片4→服务器2,3  
  - mod=3: 分片1→服务器4,1；分片2→服务器1,2；分片3→服务器2,3；分片4→服务器3,4

### 网络协议
- **数据格式**: 大端序（网络字节序）
- **整数传输**: 使用htonl/ntohl转换
- **字符串传输**: 以null结尾的C字符串
- **错误处理**: 返回-1表示错误，0表示成功

### 加密机制
- **算法**: XOR简单加密
- **密钥**: 用户密码的SHA256哈希值
- **安全性**: 适用于演示目的，生产环境应使用更强加密


## 扩展与定制

### 添加新服务器
1. 修改 `conf/dfc.conf` 添加新的Server行
2. 在 `conf/dfs.conf` 中添加用户配置
3. 更新分片算法逻辑（需要修改源代码）

### 修改端口
1. 修改 `conf/dfc.conf` 中的端口号
2. 启动服务器时指定新端口：`bin/dfs server/DFS1 20001`

### 自定义加密
修改 `src/utils.cpp` 中的加密函数：
- `encryptSplit()`: 文件分片加密
- `decryptSplit()`: 文件分片解密

## 贡献指南

欢迎提交Issue和Pull Request！请遵循以下准则：

1. **代码风格**: 使用clang-format格式化代码
2. **测试**: 确保所有现有测试通过
3. **文档**: 更新相关文档和注释
4. **提交信息**: 使用清晰的提交信息描述变更

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题或建议，请联系项目维护者。

---

**注意**: 本系统适用于学习分布式系统原理，不建议在生产环境中使用。如需生产级分布式文件系统，请考虑HDFS、Ceph、GlusterFS等成熟解决方案。