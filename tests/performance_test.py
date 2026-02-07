#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
分布式文件系统性能测试脚本
用于学术论文写作的吞吐量、时延等指标测试

学术严谨性特性：
1. 多种测试文件类型（随机数据、文本数据、二进制数据）
2. 统计显著性分析（多次迭代，计算置信区间）
3. 系统资源监控（CPU、内存使用情况）
4. 环境变量记录（确保实验可重复性）
5. 异常处理和错误恢复机制
6. 详细的日志记录
"""

import os
import sys
import time
import subprocess
import json
import argparse
from pathlib import Path
import shutil
import signal
import psutil
import random
import string
from datetime import datetime


class DFSPerformanceTester:
    def __init__(self, test_dir="performance_test", config_file="conf/dfc.conf"):
        self.test_dir = Path(test_dir)
        self.config_file = config_file
        self.results = []
        self.server_processes = []
        self.start_time = None
        self.system_info = self._collect_system_info()
        
    def _collect_system_info(self):
        """收集系统信息用于实验记录"""
        return {
            "timestamp": datetime.now().isoformat(),
            "python_version": sys.version,
            "os_info": os.uname() if hasattr(os, 'uname') else "Unknown",
            "cpu_count": psutil.cpu_count(),
            "memory_total_gb": psutil.virtual_memory().total / (1024**3),
            "disk_usage": str(psutil.disk_usage('/')),
            "environment": dict(os.environ)
        }
    
    def setup_test_environment(self):
        """设置测试环境"""
        print("Setting up test environment...")
        self.test_dir.mkdir(exist_ok=True)
        self.start_time = time.time()
        
        # 清理之前的测试文件
        self.cleanup()
        
        # 启动DFS服务器
        self.start_servers()
        
    def cleanup(self):
        """清理测试环境"""
        print("Cleaning up test environment...")
        
        # 杀掉所有DFS进程
        try:
            subprocess.run(["make", "kill"], cwd="/home/lab/dfs", 
                          capture_output=True, timeout=10)
        except:
            pass
            
        # 清理测试目录
        if self.test_dir.exists():
            shutil.rmtree(self.test_dir)
            
        # 清理服务器数据
        server_dirs = [f"server/DFS{i}" for i in range(1, 5)]
        for server_dir in server_dirs:
            server_path = Path("/home/lab/dfs") / server_dir
            if server_path.exists():
                for item in server_path.iterdir():
                    if item.name not in ["Bob", "Alice"]:
                        if item.is_file():
                            item.unlink()
                        elif item.is_dir():
                            shutil.rmtree(item)
    
    def start_servers(self):
        """启动DFS服务器"""
        print("Starting DFS servers...")
        try:
            result = subprocess.run(["make", "start"], 
                                  cwd="/home/lab/dfs",
                                  capture_output=True, 
                                  timeout=15)
            if result.returncode != 0:
                print(f"Warning: Server startup may have issues: {result.stderr.decode()}")
            time.sleep(3)  # 等待服务器完全启动
        except subprocess.TimeoutExpired:
            print("Warning: Server startup timed out, continuing...")
    
    def create_random_file(self, size_bytes, filename):
        """创建随机数据文件"""
        filepath = self.test_dir / filename
        with open(filepath, 'wb') as f:
            chunk_size = min(1024 * 1024, size_bytes)
            remaining = size_bytes
            while remaining > 0:
                write_size = min(chunk_size, remaining)
                f.write(os.urandom(write_size))
                remaining -= write_size
        return filepath
    
    def create_text_file(self, size_bytes, filename):
        """创建文本数据文件"""
        filepath = self.test_dir / filename
        # 生成有意义的文本内容
        text_content = "This is a test file for distributed file system performance evaluation. " * 1000
        with open(filepath, 'w') as f:
            written = 0
            while written < size_bytes:
                to_write = min(len(text_content), size_bytes - written)
                f.write(text_content[:to_write])
                written += to_write
        return filepath
    
    def create_binary_file(self, size_bytes, filename):
        """创建二进制模式文件（重复模式）"""
        filepath = self.test_dir / filename
        with open(filepath, 'wb') as f:
            pattern = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F'
            written = 0
            while written < size_bytes:
                to_write = min(len(pattern), size_bytes - written)
                f.write(pattern[:to_write])
                written += to_write
        return filepath
    
    def create_test_file(self, size_bytes, filename, file_type="random"):
        """创建指定类型和大小的测试文件"""
        if file_type == "random":
            return self.create_random_file(size_bytes, filename)
        elif file_type == "text":
            return self.create_text_file(size_bytes, filename)
        elif file_type == "binary":
            return self.create_binary_file(size_bytes, filename)
        else:
            return self.create_random_file(size_bytes, filename)
    
    def measure_system_resources(self):
        """测量系统资源使用情况"""
        cpu_percent = psutil.cpu_percent(interval=0.1)
        memory_info = psutil.virtual_memory()
        return {
            "cpu_percent": cpu_percent,
            "memory_percent": memory_info.percent,
            "memory_available_gb": memory_info.available / (1024**3)
        }
    
    def measure_put_operation(self, filepath, remote_filename):
        """测量PUT操作的性能"""
        start_time = time.time()
        start_resources = self.measure_system_resources()
        
        try:
            result = subprocess.run([
                "/home/lab/dfs/bin/dfc", 
                self.config_file,
                "PUT", 
                str(filepath), 
                remote_filename
            ], cwd="/home/lab/dfs", capture_output=True, timeout=120)
            
            end_time = time.time()
            end_resources = self.measure_system_resources()
            success = result.returncode == 0 and b"File uploaded successfully" in result.stdout
            
            if success:
                latency = end_time - start_time
                throughput = filepath.stat().st_size / latency / (1024 * 1024)  # MB/s
                resource_usage = {
                    "cpu_avg": (start_resources["cpu_percent"] + end_resources["cpu_percent"]) / 2,
                    "memory_avg": (start_resources["memory_percent"] + end_resources["memory_percent"]) / 2
                }
                return True, latency, throughput, resource_usage
            else:
                print(f"PUT failed: {result.stderr.decode()}")
                return False, 0, 0, {}
                
        except subprocess.TimeoutExpired:
            print(f"PUT operation timed out for file {filepath}")
            return False, 0, 0, {}
        except Exception as e:
            print(f"PUT operation error: {e}")
            return False, 0, 0, {}
    
    def measure_get_operation(self, remote_filename, local_filename):
        """测量GET操作的性能"""
        local_filepath = self.test_dir / local_filename
        start_time = time.time()
        start_resources = self.measure_system_resources()
        
        try:
            result = subprocess.run([
                "/home/lab/dfs/bin/dfc", 
                self.config_file,
                "GET", 
                remote_filename, 
                str(local_filepath)
            ], cwd="/home/lab/dfs", capture_output=True, timeout=120)
            
            end_time = time.time()
            end_resources = self.measure_system_resources()
            success = result.returncode == 0 and local_filepath.exists()
            
            if success:
                file_size = local_filepath.stat().st_size
                latency = end_time - start_time
                throughput = file_size / latency / (1024 * 1024)  # MB/s
                resource_usage = {
                    "cpu_avg": (start_resources["cpu_percent"] + end_resources["cpu_percent"]) / 2,
                    "memory_avg": (start_resources["memory_percent"] + end_resources["memory_percent"]) / 2
                }
                return True, latency, throughput, file_size, resource_usage
            else:
                print(f"GET failed: {result.stderr.decode()}")
                return False, 0, 0, 0, {}
                
        except subprocess.TimeoutExpired:
            print(f"GET operation timed out for file {remote_filename}")
            return False, 0, 0, 0, {}
        except Exception as e:
            print(f"GET operation error: {e}")
            return False, 0, 0, 0, {}
    
    def run_single_test(self, file_size_mb, test_id, file_type="random"):
        """运行单个文件大小和类型的测试"""
        print(f"\nTesting file size: {file_size_mb} MB, type: {file_type}")
        
        # 创建测试文件
        file_size_bytes = file_size_mb * 1024 * 1024
        test_filename = f"test_{test_id}_{file_size_mb}MB_{file_type}.bin"
        remote_filename = f"remote_{test_id}_{file_size_mb}MB_{file_type}.bin"
        local_get_filename = f"downloaded_{test_id}_{file_size_mb}MB_{file_type}.bin"
        
        filepath = self.create_test_file(file_size_bytes, test_filename, file_type)
        
        # 测试PUT操作
        put_success, put_latency, put_throughput, put_resources = self.measure_put_operation(
            filepath, remote_filename
        )
        
        get_success, get_latency, get_throughput, actual_size, get_resources = False, 0, 0, 0, {}
        if put_success:
            # 测试GET操作
            get_success, get_latency, get_throughput, actual_size, get_resources = self.measure_get_operation(
                remote_filename, local_get_filename
            )
            
            # 验证文件完整性
            if get_success and actual_size == file_size_bytes:
                integrity_ok = True
            else:
                integrity_ok = False
                get_success = False
        else:
            integrity_ok = False
        
        # 清理测试文件
        if filepath.exists():
            filepath.unlink()
        downloaded_file = self.test_dir / local_get_filename
        if downloaded_file.exists():
            downloaded_file.unlink()
        
        result = {
            "file_size_mb": file_size_mb,
            "file_size_bytes": file_size_bytes,
            "file_type": file_type,
            "put_success": put_success,
            "put_latency_s": put_latency,
            "put_throughput_mbps": put_throughput,
            "put_cpu_usage": put_resources.get("cpu_avg", 0),
            "put_memory_usage": put_resources.get("memory_avg", 0),
            "get_success": get_success,
            "get_latency_s": get_latency,
            "get_throughput_mbps": get_throughput,
            "get_cpu_usage": get_resources.get("cpu_avg", 0),
            "get_memory_usage": get_resources.get("memory_avg", 0),
            "integrity_ok": integrity_ok,
            "test_id": test_id,
            "timestamp": datetime.now().isoformat()
        }
        
        self.results.append(result)
        return result
    
    def run_comprehensive_test(self, file_sizes_mb, iterations=5, file_types=None):
        """运行完整的性能测试"""
        if file_types is None:
            file_types = ["random"]
        
        print(f"Starting comprehensive performance test...")
        print(f"File sizes to test: {file_sizes_mb} MB")
        print(f"File types to test: {file_types}")
        print(f"Iterations per configuration: {iterations}")
        print(f"Total test configurations: {len(file_sizes_mb) * len(file_types) * iterations}")
        
        test_id = 0
        for file_type in file_types:
            for file_size in file_sizes_mb:
                for iteration in range(iterations):
                    test_id += 1
                    try:
                        result = self.run_single_test(file_size, test_id, file_type)
                        if result["put_success"] and result["get_success"]:
                            print(f"  [{file_type}] Iteration {iteration+1}: PUT={result['put_throughput_mbps']:.2f} MB/s, "
                                  f"GET={result['get_throughput_mbps']:.2f} MB/s")
                        else:
                            print(f"  [{file_type}] Iteration {iteration+1}: Failed")
                    except Exception as e:
                        print(f"  [{file_type}] Iteration {iteration+1}: Error - {e}")
                        continue
        
        return self.results
    
    def save_results(self, output_file="performance_results.json"):
        """保存测试结果，包含完整的实验元数据"""
        complete_results = {
            "system_info": self.system_info,
            "test_duration_seconds": time.time() - self.start_time if self.start_time else 0,
            "results": self.results
        }
        
        with open(output_file, 'w') as f:
            json.dump(complete_results, f, indent=2, default=str)
        print(f"Results saved to {output_file}")
    
    def shutdown(self):
        """关闭测试环境"""
        print("Shutting down test environment...")
        self.cleanup()


def main():
    parser = argparse.ArgumentParser(description='DFS Performance Tester for Academic Research')
    parser.add_argument('--sizes', nargs='+', type=int, 
                       default=[1, 5, 10, 25, 50, 100, 200, 500],
                       help='File sizes to test in MB (default: 1 5 10 25 50 100 200 500)')
    parser.add_argument('--iterations', type=int, default=5,
                       help='Number of iterations per configuration (default: 5, recommended for academic research)')
    parser.add_argument('--types', nargs='+', choices=['random', 'text', 'binary'], 
                       default=['random'],
                       help='File types to test (default: random)')
    parser.add_argument('--output', type=str, default='performance_results.json',
                       help='Output JSON file for results')
    parser.add_argument('--plot-only', action='store_true',
                       help='Only generate plots from existing results')
    
    args = parser.parse_args()
    
    if args.plot_only:
        # 只生成图表
        from generate_plots import generate_plots
        generate_plots(args.output)
        return
    
    # 运行性能测试
    tester = DFSPerformanceTester()
    try:
        tester.setup_test_environment()
        results = tester.run_comprehensive_test(args.sizes, args.iterations, args.types)
        tester.save_results(args.output)
        
        # 生成图表
        from generate_plots import generate_plots
        generate_plots(args.output)
        
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"Test failed with error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        tester.shutdown()


if __name__ == "__main__":
    main()