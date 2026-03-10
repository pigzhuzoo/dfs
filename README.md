# Distributed File System (DFS)

A C++17 distributed file system with multi-algorithm encryption support, including FPGA hardware acceleration. Rewritten from the original [C implementation](https://github.com/Hasil-Sharma/distributed-file-system).

## Features

- **File Sharding**: Files are split into 4 shards, each stored on 2 servers for redundancy
- **Multi-Algorithm Encryption**: AES-256 (GCM/ECB/CBC/CFB/OFB/CTR), SM4 (ECB/CBC/CTR), RSA-OAEP
- **FPGA Hardware Acceleration**: Xilinx FPGA accelerated AES-256 encryption with automatic CPU fallback
- **User Authentication**: Multi-user support with isolated storage
- **Fault Tolerance**: Continue operating when up to 3 servers fail
- **AES-NI Acceleration**: Hardware-accelerated encryption when available

## Quick Start

```bash
# Build and start servers
make all

# Or step by step
make dfs dfc      # Compile server and client
make start        # Start 4 servers (ports 10001-10004)
make client       # Start interactive client

# FPGA acceleration mode
make USE_FPGA=1 dfs dfc    # Compile with FPGA support
make USE_FPGA=1 start      # Start servers
```

## Client Commands

After starting the client, use these commands at the `>>>` prompt:

### MKDIR - Create Directory
```
>>> MKDIR myfolder
>>> MKDIR documents/
```

### LIST - List Files
```
>>> LIST /
>>> LIST /myfolder
```

### PUT - Upload File
```
>>> PUT /path/to/local.txt remote.txt
>>> PUT ./document.pdf backup.pdf
```

### GET - Download File
```
>>> GET remote.txt /path/to/save.txt
>>> GET backup.pdf ./restored.pdf
```

### EXIT - Quit
```
>>> EXIT
```

## Encryption Algorithms

### Software Encryption

| Type | Key Size | IV Size | Description |
|------|----------|---------|-------------|
| `AES_256_GCM` | 256-bit | 96-bit | Recommended, authenticated encryption |
| `AES_256_ECB` | 256-bit | None | Simple but less secure |
| `AES_256_CBC` | 256-bit | 128-bit | Block cipher with padding |
| `AES_256_CTR` | 256-bit | 128-bit | Stream cipher mode |
| `SM4_CTR` | 128-bit | 128-bit | Chinese national standard |
| `RSA_OAEP` | 2048-bit | - | Asymmetric encryption for small data |

### FPGA Hardware Acceleration

| Type | Key Size | Description |
|------|----------|-------------|
| `AES_256_FPGA` | 256-bit | Xilinx FPGA accelerated, automatic CPU fallback |

Configure in `conf/dfc.conf`:
```
EncryptionType: AES_256_FPGA
```

### FPGA Configuration

Specify FPGA paths during compilation:
```bash
make USE_FPGA=1 XRT_PATH=/opt/xilinx/xrt XCLBIN_PATH=/path/to/aes256.hw.xclbin dfs dfc
```

Environment variables:
- `USE_FPGA=1`: Enable FPGA support
- `XRT_PATH`: Xilinx XRT installation path (default: `/opt/xilinx/xrt`)
- `XCLBIN_PATH`: FPGA bitstream file path

## Testing

### Functional Tests
```bash
make test              # Run all functional tests
make test-commands     # Test MKDIR, LIST commands
make test-get          # Test GET command
make test-put          # Test PUT command
make test-encryption   # Test all encryption algorithms
make test-crypto       # Test crypto implementation
```

### Performance Tests
```bash
make perf-test           # Full performance test (default)
make perf-test-quick     # Quick test (3 sizes, 3 iterations)
make perf-test-full      # Full test (7 sizes, 5 iterations)
make perf-test-fpga      # FPGA accelerated performance test
make perf-test-compare   # CPU vs FPGA comparison test
make perf-test-plots     # Generate plots from existing results
make multi-tenant-test   # Multi-tenant performance test
```

#### Test Parameters
| Parameter | Description | Default |
|-----------|-------------|---------|
| `--sizes` | File sizes in MB | 4K, 16K, 64K, 256K, 1M, 4M, 16M |
| `--iterations` | Number of iterations | 5 |
| `--warmup` | Warmup iterations (excluded from stats) | 2 |
| `--cooldown` | Cooldown time between tests (seconds) | 1 |
| `--seed` | Random seed for reproducibility | 42 |
| `--confidence-level` | Confidence interval level | 0.95 |
| `--remove-outliers` | Remove outliers using IQR method | False |

#### Output Files
- `tests/performance/plots/performance_results_cpu.json` - CPU test data
- `tests/performance/plots/performance_results_fpga.json` - FPGA test data
- `tests/performance/plots/comparison_report.json` - Comparison report
- `tests/performance/plots/throughput/` - Throughput comparison charts
- `tests/performance/plots/latency/` - Latency comparison charts
- `tests/performance/plots/cpu_usage/` - CPU usage comparison charts
- `tests/performance/plots/memory_usage/` - Memory usage comparison charts
- `tests/performance/plots/speedup/` - Speedup analysis charts
- `tests/performance/plots/resource_usage_comparison.png` - Resource usage summary

## Management

```bash
make clean    # Clean build artifacts and test outputs
make kill     # Stop all server processes
make clear    # Clear server data directories
```

## Configuration

### Client (`conf/dfc.conf`)
```
Server DFS1 127.0.0.1:10001
Server DFS2 127.0.0.1:10002
Server DFS3 127.0.0.1:10003
Server DFS4 127.0.0.1:10004

Username: Bob
Password: ComplextPassword
EncryptionType: AES_256_FPGA
```

### Encryption Type Options
```
# 0 or AES_256_GCM - AES-256-GCM (recommended)
# 1 or AES_256_ECB - AES-256-ECB
# 2 or AES_256_CBC - AES-256-CBC
# 3 or AES_256_CFB - AES-256-CFB
# 4 or AES_256_OFB - AES-256-OFB
# 5 or AES_256_CTR - AES-256-CTR
# 6 or SM4_ECB - SM4-ECB
# 7 or SM4_CBC - SM4-CBC
# 8 or SM4_CTR - SM4-CTR
# 9 or RSA_OAEP - RSA-OAEP
# 10 or AES_256_FPGA - FPGA accelerated AES-256
```

## Requirements

### Basic Requirements
- C++17 compiler (clang++)
- OpenSSL 1.1.0+
- GNU Make
- Python 3.6+ (for performance tests)
- matplotlib, numpy, pandas, psutil, scipy (for plotting)

```bash
sudo apt-get install libssl-dev python3-pip
pip3 install matplotlib numpy pandas psutil scipy
```

### FPGA Support (Optional)
- Xilinx XRT (Xilinx Runtime)
- Xilinx FPGA accelerator card (e.g., Alveo U50, U200)
- FPGA bitstream file (aes256.hw.xclbin)

```bash
# Install XRT
sudo apt install xrt
```

## Project Structure

```
dfs/
├── src/                    # Source files
│   ├── client/            # Client code
│   ├── server/            # Server code
│   ├── common/            # Common utilities
│   └── crypto/            # Encryption module
│       ├── crypto_utils.cpp    # Crypto utilities
│       └── fpga_aes.cpp        # FPGA acceleration module
├── include/               # Header files
│   └── crypto/
│       ├── crypto_utils.hpp    # Crypto interface
│       └── fpga_aes.hpp        # FPGA interface
├── conf/                  # Configuration files
├── tests/                 # Test scripts
│   ├── performance/       # Performance tests
│   │   ├── performance_test.py    # Main test script
│   │   ├── compare_performance.py # CPU vs FPGA comparison
│   │   ├── generate_plots.py      # Plot generation
│   │   └── non_interactive_client.py
│   ├── integration/       # Integration tests
│   └── unit/              # Unit tests
├── server/                # Data storage (DFS1-4)
├── logs/                  # Server logs
└── Makefile              # Build configuration
```

## Performance Results

### CPU vs FPGA Performance Comparison (2026-03-10)

#### Throughput Comparison

| File Size | CPU PUT (MB/s) | FPGA PUT (MB/s) | PUT Speedup | CPU GET (MB/s) | FPGA GET (MB/s) | GET Speedup |
|-----------|----------------|-----------------|-------------|----------------|-----------------|-------------|
| 4 KB | 0.022 | 0.021 | 0.95x | 0.015 | 0.015 | 1.00x |
| 16 KB | 0.084 | 0.084 | 1.00x | 0.058 | 0.057 | 0.98x |
| 64 KB | 0.331 | 0.327 | 0.99x | 0.272 | 0.279 | 1.03x |
| 256 KB | 1.30 | 1.27 | 0.98x | 0.92 | 0.91 | 0.98x |
| 1 MB | 4.65 | 4.66 | 1.00x | 4.05 | 3.89 | 0.96x |
| 4 MB | 15.45 | 13.98 | 0.90x | 14.75 | 14.58 | 0.99x |
| 16 MB | 41.04 | 30.75 | 0.75x | 30.79 | 30.81 | 1.00x |

#### Latency Comparison

| File Size | CPU PUT Latency (ms) | FPGA PUT Latency (ms) | CPU GET Latency (ms) | FPGA GET Latency (ms) |
|-----------|----------------------|-----------------------|----------------------|-----------------------|
| 4 KB | 185 | 195 | 272 | 274 |
| 16 KB | 192 | 191 | 277 | 282 |
| 64 KB | 193 | 198 | 235 | 229 |
| 256 KB | 198 | 204 | 278 | 282 |
| 1 MB | 215 | 215 | 247 | 258 |
| 4 MB | 259 | 286 | 271 | 275 |
| 16 MB | 392 | 521 | 520 | 519 |

#### Resource Utilization

| File Size | CPU PUT Usage | FPGA PUT Usage | CPU GET Usage | FPGA GET Usage | Memory Usage |
|-----------|---------------|----------------|---------------|----------------|--------------|
| 4 KB | 2.9% | 4.1% | 2.95% | 3.21% | 6.1% |
| 1 MB | 2.84% | 3.36% | 3.06% | 4.16% | 6.1% |
| 16 MB | 2.10% | 2.06% | 3.16% | 3.49% | 6.1% |

### FPGA Performance Analysis

#### Why is FPGA acceleration not significant?

Reasons why FPGA performance is slightly lower than CPU in current implementation:

1. **PCIe Transfer Overhead**: Data requires CPU→FPGA→CPU bidirectional transfer; for small data, transfer time exceeds computation time
2. **Small Object Size**: Default 64KB objects cannot fully utilize FPGA parallelism
3. **OpenCL Launch Latency**: Each encryption has 10-50ms kernel launch overhead
4. **No Batch Processing**: Each object is encrypted separately, preventing pipelining

#### Performance Bottleneck Breakdown (16MB file)

```
CPU encryption time: ~392 ms
FPGA total time: ~521 ms
├── PCIe transfer: ~400 ms (estimated)
├── FPGA compute: ~50 ms (estimated)
└── OpenCL overhead: ~70 ms (estimated)
```

#### Optimization Recommendations

| Optimization | Expected Speedup | Difficulty |
|--------------|------------------|------------|
| Batch process multiple objects | 2-3x | Medium |
| Increase object size to 1MB | 1.5-2x | Low |
| DMA transfer | 1.2-1.5x | High |
| Pipeline processing | 1.5-2x | Medium |
| Combined optimizations | 3-5x | High |

### Multi-Tenant Performance

| Tenants | P50 Latency | P95 Latency | P99 Latency |
|---------|-------------|-------------|-------------|
| 1 | 343 ms | 343 ms | 343 ms |
| 2 | 763 ms | 818 ms | 818 ms |
| 3 | 838 ms | 858 ms | 858 ms |
| 4 | 5026 ms | 5443 ms | 5443 ms |

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           DFS System Architecture                            │
└─────────────────────────────────────────────────────────────────────────────┘

  Client (DFC)                                          Server (DFS)
  ┌─────────────────┐                                  ┌─────────────────┐
  │                 │                                  │                 │
  │  ┌───────────┐  │                                  │                 │
  │  │ File Split │  │                                  │                 │
  │  └─────┬─────┘  │                                  │                 │
  │        │        │                                  │                 │
  │        ▼        │                                  │                 │
  │  ┌───────────┐  │     PUT/GET/LIST/MKDIR          │                 │
  │  │ Enc/Dec   │──┼─────────────────────────────────▶│   Store shards  │
  │  └─────┬─────┘  │                                  │   (each stored  │
  │        │        │                                  │    twice)       │
  │   ┌────┴────┐   │                                  │                 │
  │   │         │   │                                  │                 │
  │   ▼         ▼   │                                  │                 │
  │ CPU Enc   FPGA Enc│                                  │                 │
  │ (OpenSSL) (XRT) │                                  │                 │
  │                 │                                  │                 │
  └─────────────────┘                                  └─────────────────┘
```

### Encryption Flow

```
PUT Operation:
  1. splitFileToPieces() - Split file into pieces
  2. encryptDecryptFileSplit() - Encrypt pieces
     ├── AES_256_ECB → CPU encryption
     └── AES_256_FPGA → FPGA encryption (if available) or fallback to CPU
  3. sendFileSplits() - Send to servers

GET Operation:
  1. fetchRemoteSplits() - Fetch from servers
  2. encryptDecryptFileSplit() - Decrypt pieces
  3. combineFileFromPieces() - Combine into file
```

### FPGA Encryption Call Chain

```
dfc client
  ↓
DfcUtils::commandExec() (PUT command)
  ↓
Utils::encryptDecryptFileSplit()
  ↓
CryptoUtils::encryptData()
  ↓
case AES_256_FPGA:
  ├── FpgaAes::isAvailable() → Detect FPGA
  │   ├── FPGA available → FpgaAes::encrypt()
  │   └── FPGA unavailable → Fallback to CPU encryption
  └── return result
```

## Academic Citation

If you use this project in your research, please cite using the following format:

```bibtex
@software{dfs2026,
  title = {Distributed File System with Multi-Algorithm Encryption and FPGA Acceleration},
  author = {DFS Project},
  year = {2026},
  url = {https://github.com/your-username/distributed-file-system}
}
```

## License

For learning and research purposes only.
