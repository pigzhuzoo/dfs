# 分布式文件系统 (DFS)

基于 C++17 的分布式文件系统，支持多种加密算法，包括FPGA硬件加速。从原始 [C 语言实现](https://github.com/Hasil-Sharma/distributed-file-system) 重写而来。

## 功能特性

- **文件分片**: 文件被分割为 4 个分片，每个分片存储在 2 个服务器上以实现冗余
- **多算法加密**: AES-256 (GCM/ECB/CBC/CFB/OFB/CTR), SM4 (ECB/CBC/CTR), RSA-OAEP
- **FPGA硬件加速**: 支持Xilinx FPGA加速AES-256加密，自动回退到CPU
- **用户认证**: 支持多用户，存储空间隔离
- **容错能力**: 最多 3 个服务器故障时仍可正常运行
- **AES-NI 加速**: 支持CPU硬件加速加密

## 快速开始

```bash
# 编译并启动服务器
make all

# 或分步执行
make dfs dfc      # 编译服务器和客户端
make start        # 启动4个服务器 (端口 10001-10004)
make client       # 启动交互式客户端

# FPGA加速模式
make USE_FPGA=1 dfs dfc    # 编译带FPGA支持的版本
make USE_FPGA=1 start      # 启动服务器
```

## 客户端命令

启动客户端后，在 `>>>` 提示符下使用以下命令：

### MKDIR - 创建目录
```
>>> MKDIR myfolder
>>> MKDIR documents/
```

### LIST - 列出文件
```
>>> LIST /
>>> LIST /myfolder
```

### PUT - 上传文件
```
>>> PUT /path/to/local.txt remote.txt
>>> PUT ./document.pdf backup.pdf
```

### GET - 下载文件
```
>>> GET remote.txt /path/to/save.txt
>>> GET backup.pdf ./restored.pdf
```

### EXIT - 退出
```
>>> EXIT
```

## 加密算法

### 软件加密

| 类型 | 密钥长度 | IV长度 | 说明 |
|------|----------|--------|------|
| `AES_256_GCM` | 256位 | 96位 | 推荐，认证加密 |
| `AES_256_ECB` | 256位 | 无 | 简单但安全性较低 |
| `AES_256_CBC` | 256位 | 128位 | 块加密，需要填充 |
| `AES_256_CTR` | 256位 | 128位 | 流密码模式 |
| `SM4_CTR` | 128位 | 128位 | 国密算法 |
| `RSA_OAEP` | 2048位 | - | 非对称加密，仅用于小数据 |

### FPGA硬件加速

| 类型 | 密钥长度 | 说明 |
|------|----------|------|
| `AES_256_FPGA` | 256位 | Xilinx FPGA加速，自动回退到CPU |

在 `conf/dfc.conf` 中配置：
```
EncryptionType: AES_256_FPGA
```

### FPGA配置

编译时指定FPGA相关路径：
```bash
make USE_FPGA=1 XRT_PATH=/opt/xilinx/xrt XCLBIN_PATH=/path/to/aes256.hw.xclbin dfs dfc
```

环境变量：
- `USE_FPGA=1`: 启用FPGA支持
- `XRT_PATH`: Xilinx XRT安装路径 (默认: `/opt/xilinx/xrt`)
- `XCLBIN_PATH`: FPGA比特流文件路径

## 测试

### 功能测试
```bash
make test              # 运行所有功能测试
make test-commands     # 测试 MKDIR, LIST 命令
make test-get          # 测试 GET 命令
make test-put          # 测试 PUT 命令
make test-encryption   # 测试所有加密算法
make test-crypto       # 测试加密实现
```

### 性能测试
```bash
make perf-test           # 完整性能测试 (默认)
make perf-test-quick     # 快速测试 (3个文件大小, 3次迭代)
make perf-test-full      # 完整测试 (7个文件大小, 5次迭代)
make perf-test-fpga      # FPGA加速性能测试
make perf-test-compare   # CPU vs FPGA性能对比测试
make perf-test-plots     # 从现有结果生成图表
make multi-tenant-test   # 多租户性能测试
```

#### 测试参数
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--sizes` | 文件大小 (MB) | 4K, 16K, 64K, 256K, 1M, 4M, 16M |
| `--iterations` | 迭代次数 | 5 |
| `--warmup` | 预热迭代次数 (不计入统计) | 2 |
| `--cooldown` | 测试间冷却时间 (秒) | 1 |
| `--seed` | 随机种子 (确保可重复性) | 42 |
| `--confidence-level` | 置信区间水平 | 0.95 |
| `--remove-outliers` | 使用 IQR 方法剔除异常值 | False |

#### 输出文件
- `tests/performance/plots/performance_results_cpu.json` - CPU测试数据
- `tests/performance/plots/performance_results_fpga.json` - FPGA测试数据
- `tests/performance/plots/comparison_report.json` - 对比报告
- `tests/performance/plots/throughput/` - 吞吐量对比图
- `tests/performance/plots/latency/` - 延迟对比图
- `tests/performance/plots/cpu_usage/` - CPU利用率对比图
- `tests/performance/plots/memory_usage/` - 内存利用率对比图
- `tests/performance/plots/speedup/` - 加速比分析图
- `tests/performance/plots/resource_usage_comparison.png` - 资源使用综合对比图

## 管理

```bash
make clean    # 清理编译产物和测试输出
make kill     # 停止所有服务器进程
make clear    # 清空服务器数据目录
```

## 配置

### 客户端 (`conf/dfc.conf`)
```
Server DFS1 127.0.0.1:10001
Server DFS2 127.0.0.1:10002
Server DFS3 127.0.0.1:10003
Server DFS4 127.0.0.1:10004

Username: Bob
Password: ComplextPassword
EncryptionType: AES_256_FPGA
```

### 加密类型选项
```
# 0 或 AES_256_GCM - AES-256-GCM (推荐)
# 1 或 AES_256_ECB - AES-256-ECB
# 2 或 AES_256_CBC - AES-256-CBC
# 3 或 AES_256_CFB - AES-256-CFB
# 4 或 AES_256_OFB - AES-256-OFB
# 5 或 AES_256_CTR - AES-256-CTR
# 6 或 SM4_ECB - SM4-ECB
# 7 或 SM4_CBC - SM4-CBC
# 8 或 SM4_CTR - SM4-CTR
# 9 或 RSA_OAEP - RSA-OAEP
# 10 或 AES_256_FPGA - FPGA加速AES-256
```

## 环境要求

### 基本要求
- C++17 编译器 (clang++)
- OpenSSL 1.1.0+
- GNU Make
- Python 3.6+ (性能测试)
- matplotlib, numpy, pandas, psutil, scipy (绘图)

```bash
sudo apt-get install libssl-dev python3-pip
pip3 install matplotlib numpy pandas psutil scipy
```

### FPGA支持 (可选)
- Xilinx XRT (Xilinx Runtime)
- Xilinx FPGA加速卡 (如 Alveo U50, U200)
- FPGA比特流文件 (aes256.hw.xclbin)

```bash
# 安装XRT
sudo apt install xrt
```

## 项目结构

```
dfs/
├── src/                    # 源文件
│   ├── client/            # 客户端代码
│   ├── server/            # 服务器代码
│   ├── common/            # 公共工具
│   └── crypto/            # 加密模块
│       ├── crypto_utils.cpp    # 加密工具类
│       └── fpga_aes.cpp        # FPGA加速模块
├── include/               # 头文件
│   └── crypto/
│       ├── crypto_utils.hpp    # 加密接口
│       └── fpga_aes.hpp        # FPGA接口
├── conf/                  # 配置文件
├── tests/                 # 测试脚本
│   ├── performance/       # 性能测试
│   │   ├── performance_test.py    # 主测试脚本
│   │   ├── compare_performance.py # CPU vs FPGA对比
│   │   ├── generate_plots.py      # 图表生成
│   │   └── non_interactive_client.py
│   ├── integration/       # 集成测试
│   └── unit/              # 单元测试
├── server/                # 数据存储 (DFS1-4)
├── logs/                  # 服务器日志
└── Makefile              # 构建配置
```

## 性能测试结果

### CPU vs FPGA 性能对比 (2026-03-10)

#### 吞吐量对比

| 文件大小 | CPU PUT (MB/s) | FPGA PUT (MB/s) | PUT加速比 | CPU GET (MB/s) | FPGA GET (MB/s) | GET加速比 |
|---------|----------------|-----------------|-----------|----------------|-----------------|-----------|
| 4 KB | 0.022 | 0.021 | 0.95x | 0.015 | 0.015 | 1.00x |
| 16 KB | 0.084 | 0.084 | 1.00x | 0.058 | 0.057 | 0.98x |
| 64 KB | 0.331 | 0.327 | 0.99x | 0.272 | 0.279 | 1.03x |
| 256 KB | 1.30 | 1.27 | 0.98x | 0.92 | 0.91 | 0.98x |
| 1 MB | 4.65 | 4.66 | 1.00x | 4.05 | 3.89 | 0.96x |
| 4 MB | 15.45 | 13.98 | 0.90x | 14.75 | 14.58 | 0.99x |
| 16 MB | 41.04 | 30.75 | 0.75x | 30.79 | 30.81 | 1.00x |

#### 延迟对比

| 文件大小 | CPU PUT延迟 (ms) | FPGA PUT延迟 (ms) | CPU GET延迟 (ms) | FPGA GET延迟 (ms) |
|---------|------------------|-------------------|------------------|-------------------|
| 4 KB | 185 | 195 | 272 | 274 |
| 16 KB | 192 | 191 | 277 | 282 |
| 64 KB | 193 | 198 | 235 | 229 |
| 256 KB | 198 | 204 | 278 | 282 |
| 1 MB | 215 | 215 | 247 | 258 |
| 4 MB | 259 | 286 | 271 | 275 |
| 16 MB | 392 | 521 | 520 | 519 |

#### 资源利用率

| 文件大小 | CPU PUT利用率 | FPGA PUT利用率 | CPU GET利用率 | FPGA GET利用率 | 内存利用率 |
|---------|---------------|----------------|---------------|----------------|-----------|
| 4 KB | 2.9% | 4.1% | 2.95% | 3.21% | 6.1% |
| 1 MB | 2.84% | 3.36% | 3.06% | 4.16% | 6.1% |
| 16 MB | 2.10% | 2.06% | 3.16% | 3.49% | 6.1% |

### FPGA性能分析

#### 为什么FPGA加速效果不明显？

当前实现中FPGA性能反而略低于CPU的原因：

1. **PCIe传输开销**: 数据需要CPU→FPGA→CPU双向传输，对于小数据量，传输时间远大于计算时间
2. **Object Size太小**: 默认64KB的object无法充分利用FPGA并行性
3. **OpenCL启动延迟**: 每次加密有10-50ms的内核启动开销
4. **没有批量处理**: 每个object单独加密，无法流水线

#### 性能瓶颈分解 (16MB文件)

```
CPU加密时间: ~392 ms
FPGA总时间: ~521 ms
├── PCIe传输: ~400 ms (估计)
├── FPGA计算: ~50 ms (估计)
└── OpenCL开销: ~70 ms (估计)
```

#### 优化建议

| 优化措施 | 预期加速比 | 实现难度 |
|----------|-----------|----------|
| 批量处理多个object | 2-3x | 中 |
| 增大object size到1MB | 1.5-2x | 低 |
| DMA传输 | 1.2-1.5x | 高 |
| 流水线处理 | 1.5-2x | 中 |
| 综合优化 | 3-5x | 高 |

### 多租户性能

| 租户数 | P50 延迟 | P95 延迟 | P99 延迟 |
|--------|----------|----------|----------|
| 1 | 343 ms | 343 ms | 343 ms |
| 2 | 763 ms | 818 ms | 818 ms |
| 3 | 838 ms | 858 ms | 858 ms |
| 4 | 5026 ms | 5443 ms | 5443 ms |

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           DFS 系统架构                                       │
└─────────────────────────────────────────────────────────────────────────────┘

  客户端 (DFC)                                          服务器 (DFS)
  ┌─────────────────┐                                  ┌─────────────────┐
  │                 │                                  │                 │
  │  ┌───────────┐  │                                  │                 │
  │  │ 文件分片   │  │                                  │                 │
  │  └─────┬─────┘  │                                  │                 │
  │        │        │                                  │                 │
  │        ▼        │                                  │                 │
  │  ┌───────────┐  │     PUT/GET/LIST/MKDIR          │                 │
  │  │ 加密/解密  │──┼─────────────────────────────────▶│   存储分片      │
  │  └─────┬─────┘  │                                  │   (每个分片     │
  │        │        │                                  │    存2份)       │
  │   ┌────┴────┐   │                                  │                 │
  │   │         │   │                                  │                 │
  │   ▼         ▼   │                                  │                 │
  │ CPU加密  FPGA加密│                                  │                 │
  │ (OpenSSL) (XRT) │                                  │                 │
  │                 │                                  │                 │
  └─────────────────┘                                  └─────────────────┘
```

### 加密流程

```
PUT操作:
  1. splitFileToPieces() - 文件分片
  2. encryptDecryptFileSplit() - 加密分片
     ├── AES_256_ECB → CPU加密
     └── AES_256_FPGA → FPGA加密 (如果可用) 或 回退到CPU
  3. sendFileSplits() - 发送到服务器

GET操作:
  1. fetchRemoteSplits() - 从服务器获取
  2. encryptDecryptFileSplit() - 解密分片
  3. combineFileFromPieces() - 合并文件
```

### FPGA加密调用链

```
dfc客户端
  ↓
DfcUtils::commandExec() (PUT命令)
  ↓
Utils::encryptDecryptFileSplit()
  ↓
CryptoUtils::encryptData()
  ↓
case AES_256_FPGA:
  ├── FpgaAes::isAvailable() → 检测FPGA
  │   ├── FPGA可用 → FpgaAes::encrypt()
  │   └── FPGA不可用 → 回退到CPU加密
  └── return result
```

## 学术引用

如果您在研究中使用本项目，请按以下格式引用：

```bibtex
@software{dfs2026,
  title = {Distributed File System with Multi-Algorithm Encryption and FPGA Acceleration},
  author = {DFS Project},
  year = {2026},
  url = {https://github.com/your-username/distributed-file-system}
}
```

## 许可证

仅供学习和研究使用。
