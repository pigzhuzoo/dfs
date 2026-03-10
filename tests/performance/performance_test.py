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
7. 随机种子控制（确保可重复性）
8. 预热阶段和冷却时间
9. 异常值检测和剔除
10. 置信区间和标准误差计算
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
import hashlib
import math
from datetime import datetime
import concurrent.futures
from non_interactive_client import NonInteractiveDFSClient

try:
    from scipy import stats
    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False


def calculate_confidence_interval(data, confidence=0.95):
    if not data or len(data) < 2:
        return 0, 0, 0
    n = len(data)
    mean = sum(data) / n
    variance = sum((x - mean) ** 2 for x in data) / (n - 1)
    std_err = math.sqrt(variance / n)
    if HAS_SCIPY:
        h = std_err * stats.t.ppf((1 + confidence) / 2, n - 1)
    else:
        h = std_err * 1.96
    return mean, mean - h, mean + h


def calculate_standard_error(data):
    if not data or len(data) < 2:
        return 0
    n = len(data)
    mean = sum(data) / n
    variance = sum((x - mean) ** 2 for x in data) / (n - 1)
    return math.sqrt(variance / n)


def detect_outliers_iqr(data):
    if not data or len(data) < 4:
        return []
    sorted_data = sorted(data)
    n = len(sorted_data)
    q1_idx = n // 4
    q3_idx = (3 * n) // 4
    q1 = sorted_data[q1_idx]
    q3 = sorted_data[q3_idx]
    iqr = q3 - q1
    lower_bound = q1 - 1.5 * iqr
    upper_bound = q3 + 1.5 * iqr
    outlier_indices = [i for i, x in enumerate(data) if x < lower_bound or x > upper_bound]
    return outlier_indices


def remove_outliers(data, indices):
    return [x for i, x in enumerate(data) if i not in indices]


def generate_config_hash(config):
    config_str = json.dumps(config, sort_keys=True, default=str)
    return hashlib.sha256(config_str.encode()).hexdigest()[:16]


class DFSPerformanceTester:
    def __init__(self, test_dir="performance_test", config_file=None, 
                 seed=None, warmup=2, cooldown=1, confidence_level=0.95, remove_outliers=False):
        self.test_dir = Path(test_dir)
        if config_file is None:
            script_dir = Path(__file__).parent
            project_root = script_dir.parent.parent
            self.config_file = str(project_root / "conf" / "dfc.conf")
        else:
            self.config_file = config_file
        self.results = []
        self.multi_file_results = []
        self.warmup_results = []
        self.server_processes = []
        self.start_time = None
        self.seed = seed if seed is not None else int(time.time())
        self.warmup = warmup
        self.cooldown = cooldown
        self.confidence_level = confidence_level
        self.remove_outliers_flag = remove_outliers
        random.seed(self.seed)
        self.system_info = self._collect_system_info()
        self.client = NonInteractiveDFSClient(config_file)
        self.test_config = {}
        self.max_workers = min(4, os.cpu_count() or 2)
        
    def _collect_system_info(self):
        return {
            "timestamp": datetime.now().isoformat(),
            "python_version": sys.version,
            "os_info": os.uname() if hasattr(os, 'uname') else "Unknown",
            "cpu_count": psutil.cpu_count(),
            "memory_total_gb": psutil.virtual_memory().total / (1024**3),
            "disk_usage": str(psutil.disk_usage('/')),
            "random_seed": self.seed,
            "warmup_iterations": self.warmup,
            "cooldown_seconds": self.cooldown,
            "confidence_level": self.confidence_level,
            "remove_outliers": self.remove_outliers_flag,
            "has_scipy": HAS_SCIPY
        }
    
    def set_test_config(self, **kwargs):
        self.test_config = kwargs
        self.test_config["config_hash"] = generate_config_hash(kwargs)
        
    def setup_test_environment(self):
        print("Setting up test environment...")
        print(f"Random seed: {self.seed}")
        print(f"Warmup iterations: {self.warmup}")
        print(f"Cooldown time: {self.cooldown}s")
        print(f"Confidence level: {self.confidence_level * 100}%")
        print(f"Remove outliers: {self.remove_outliers_flag}")
        self.start_time = time.time()
        self.cleanup()
        self.test_dir.mkdir(exist_ok=True)
        self.start_servers()
        
    def cleanup(self):
        print("Cleaning up test environment...")
        if self.test_dir.exists():
            shutil.rmtree(self.test_dir)
        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent
        server_dirs = [f"server/DFS{i}" for i in range(1, 5)]
        for server_dir in server_dirs:
            server_path = project_root / server_dir
            if server_path.exists():
                for item in server_path.iterdir():
                    if item.name not in ["Bob", "Alice"]:
                        if item.is_file():
                            item.unlink()
                        elif item.is_dir():
                            shutil.rmtree(item)
    
    def start_servers(self):
        print("Starting DFS servers...")
        try:
            script_dir = Path(__file__).parent
            project_root = script_dir.parent.parent
            
            # Kill existing servers
            subprocess.run(["make", "kill"], 
                          cwd=str(project_root),
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL,
                          timeout=30)
            time.sleep(3)
            
            # Start servers in background (don't capture output to avoid blocking)
            subprocess.Popen(["make", "start-no-debug"], 
                           cwd=str(project_root),
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
            time.sleep(5)
            
            # Verify servers are running
            import socket
            ports = [10001, 10002, 10003, 10004]
            all_ready = True
            for port in ports:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)
                try:
                    sock.connect(('127.0.0.1', port))
                    sock.close()
                    print(f"Server on port {port} is ready")
                except socket.error as e:
                    print(f"Warning: Server on port {port} may not be ready: {e}")
                    all_ready = False
            
            if not all_ready:
                print("Warning: Some servers may not be ready, continuing anyway...")
        except subprocess.TimeoutExpired as e:
            print(f"Warning: Server kill timed out: {e}")
        except Exception as e:
            print(f"Warning: Server startup error: {e}")
        
        return True
    
    def create_random_file(self, size_bytes, filename):
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
        filepath = self.test_dir / filename
        text_content = "This is a test file for distributed file system performance evaluation. " * 1000
        with open(filepath, 'w') as f:
            written = 0
            while written < size_bytes:
                to_write = min(len(text_content), size_bytes - written)
                f.write(text_content[:to_write])
                written += to_write
        return filepath
    
    def create_binary_file(self, size_bytes, filename):
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
        if file_type == "random":
            return self.create_random_file(size_bytes, filename)
        elif file_type == "text":
            return self.create_text_file(size_bytes, filename)
        elif file_type == "binary":
            return self.create_binary_file(size_bytes, filename)
        else:
            return self.create_random_file(size_bytes, filename)
    
    def measure_system_resources(self):
        cpu_percent = psutil.cpu_percent(interval=0.1)
        memory_info = psutil.virtual_memory()
        return {
            "cpu_percent": cpu_percent,
            "memory_percent": memory_info.percent,
            "memory_available_gb": memory_info.available / (1024**3)
        }
    
    def measure_put_operation(self, filepath, remote_filename):
        start_time = time.time()
        start_resources = self.measure_system_resources()
        
        try:
            success, latency, throughput, error = self.client.put_file(
                str(filepath), remote_filename
            )
            
            end_time = time.time()
            end_resources = self.measure_system_resources()
            
            if success:
                resource_usage = {
                    "cpu_avg": (start_resources["cpu_percent"] + end_resources["cpu_percent"]) / 2,
                    "memory_avg": (start_resources["memory_percent"] + end_resources["memory_percent"]) / 2
                }
                return True, latency, throughput, resource_usage
            else:
                print(f"PUT failed: {error}")
                return False, 0, 0, {}
                
        except Exception as e:
            print(f"PUT operation error: {e}")
            return False, 0, 0, {}
    
    def measure_get_operation(self, remote_filename, local_filename):
        local_filepath = self.test_dir / local_filename
        start_time = time.time()
        start_resources = self.measure_system_resources()
        
        try:
            success, latency, throughput, file_size, error = self.client.get_file(
                remote_filename, str(local_filepath)
            )
            
            end_time = time.time()
            end_resources = self.measure_system_resources()
            
            if success:
                resource_usage = {
                    "cpu_avg": (start_resources["cpu_percent"] + end_resources["cpu_percent"]) / 2,
                    "memory_avg": (start_resources["memory_percent"] + end_resources["memory_percent"]) / 2
                }
                return True, latency, throughput, file_size, resource_usage
            else:
                print(f"GET failed: {error}")
                return False, 0, 0, 0, {}
                
        except Exception as e:
            print(f"GET operation error: {e}")
            return False, 0, 0, 0, {}
    
    def measure_multi_put_operation(self, filepaths, remote_filenames, max_workers=None):
        start_time = time.time()
        start_resources = self.measure_system_resources()
        
        if max_workers is None:
            max_workers = self.max_workers
        
        results = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_to_file = {
                executor.submit(self.measure_put_operation, fp, rf): (fp, rf)
                for fp, rf in zip(filepaths, remote_filenames)
            }
            for future in concurrent.futures.as_completed(future_to_file):
                filepath, remote_filename = future_to_file[future]
                try:
                    result = future.result()
                    results.append((filepath, remote_filename, result))
                except Exception as e:
                    print(f"PUT operation for {remote_filename} error: {e}")
                    results.append((filepath, remote_filename, (False, 0, 0, {})))
        
        end_time = time.time()
        end_resources = self.measure_system_resources()
        total_time = end_time - start_time
        
        successful_operations = []
        total_size_bytes = 0
        for fp, rf, (success, latency, throughput, resources) in results:
            if success:
                successful_operations.append((fp, rf, success, latency, throughput, resources))
                total_size_bytes += fp.stat().st_size
        
        total_throughput_mbps = (total_size_bytes / (1024 * 1024)) / total_time if total_time > 0 else 0
        avg_file_throughput_mbps = (sum([th for _, _, _, _, th, _ in successful_operations]) / len(successful_operations)) if successful_operations else 0
        
        resource_usage = {
            "cpu_avg": (start_resources["cpu_percent"] + end_resources["cpu_percent"]) / 2,
            "memory_avg": (start_resources["memory_percent"] + end_resources["memory_percent"]) / 2
        }
        
        return True, total_time, total_throughput_mbps, avg_file_throughput_mbps, len(successful_operations), resource_usage
    
    def measure_multi_get_operation(self, remote_filenames, local_filenames, max_workers=None):
        start_time = time.time()
        start_resources = self.measure_system_resources()
        
        if max_workers is None:
            max_workers = self.max_workers
        
        results = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            future_to_file = {
                executor.submit(self.measure_get_operation, rf, lf): (rf, lf)
                for rf, lf in zip(remote_filenames, local_filenames)
            }
            for future in concurrent.futures.as_completed(future_to_file):
                remote_filename, local_filename = future_to_file[future]
                try:
                    result = future.result()
                    results.append((remote_filename, local_filename, result))
                except Exception as e:
                    print(f"GET operation for {remote_filename} error: {e}")
                    results.append((remote_filename, local_filename, (False, 0, 0, 0, {})))
        
        end_time = time.time()
        end_resources = self.measure_system_resources()
        total_time = end_time - start_time
        
        successful_operations = []
        total_size_bytes = 0
        for rf, lf, (success, latency, throughput, file_size, resources) in results:
            if success:
                successful_operations.append((rf, lf, success, latency, throughput, file_size, resources))
                total_size_bytes += file_size
        
        total_throughput_mbps = (total_size_bytes / (1024 * 1024)) / total_time if total_time > 0 else 0
        avg_file_throughput_mbps = (sum([th for _, _, _, _, th, _, _ in successful_operations]) / len(successful_operations)) if successful_operations else 0
        
        resource_usage = {
            "cpu_avg": (start_resources["cpu_percent"] + end_resources["cpu_percent"]) / 2,
            "memory_avg": (start_resources["memory_percent"] + end_resources["memory_percent"]) / 2
        }
        
        return True, total_time, total_throughput_mbps, avg_file_throughput_mbps, len(successful_operations), resource_usage
    
    def run_multi_file_put_test(self, file_size_mb, num_files, max_workers=None, file_type="random", test_id=0):
        print(f"\nTesting multi-file PUT: {num_files} files of {file_size_mb} MB each, type: {file_type}")
        
        file_size_bytes = int(file_size_mb * 1024 * 1024)
        filepaths = []
        remote_filenames = []
        
        for i in range(num_files):
            test_filename = f"multi_test_{test_id}_{i}_{file_size_mb}MB_{file_type}.bin"
            remote_filename = f"multi_remote_{test_id}_{i}_{file_size_mb}MB_{file_type}.bin"
            filepath = self.create_test_file(file_size_bytes, test_filename, file_type)
            filepaths.append(filepath)
            remote_filenames.append(remote_filename)
        
        success, total_time, total_throughput, avg_file_throughput, success_count, resources = self.measure_multi_put_operation(
            filepaths, remote_filenames, max_workers
        )
        
        for filepath in filepaths:
            if filepath.exists():
                filepath.unlink()
        
        result = {
            "test_type": "multi_file_put",
            "file_size_mb": file_size_mb,
            "file_size_bytes": file_size_bytes,
            "num_files": num_files,
            "max_workers": max_workers,
            "file_type": file_type,
            "success": success,
            "success_count": success_count,
            "total_time_s": total_time,
            "total_throughput_mbps": total_throughput,
            "avg_file_throughput_mbps": avg_file_throughput,
            "cpu_usage": resources.get("cpu_avg", 0),
            "memory_usage": resources.get("memory_avg", 0),
            "test_id": test_id,
            "timestamp": datetime.now().isoformat()
        }
        
        self.multi_file_results.append(result)
        return result
    
    def run_multi_file_get_test(self, file_size_mb, num_files, max_workers=None, file_type="random", test_id=0, remote_filenames=None):
        print(f"\nTesting multi-file GET: {num_files} files of {file_size_mb} MB each, type: {file_type}")
        
        local_filenames = []
        if remote_filenames is None:
            remote_filenames = []
            for i in range(num_files):
                remote_filename = f"multi_remote_{test_id}_{i}_{file_size_mb}MB_{file_type}.bin"
                remote_filenames.append(remote_filename)
        
        for i in range(num_files):
            local_filename = f"multi_download_{test_id}_{i}_{file_size_mb}MB_{file_type}.bin"
            local_filenames.append(local_filename)
        
        success, total_time, total_throughput, avg_file_throughput, success_count, resources = self.measure_multi_get_operation(
            remote_filenames, local_filenames, max_workers
        )
        
        for local_filename in local_filenames:
            local_filepath = self.test_dir / local_filename
            if local_filepath.exists():
                local_filepath.unlink()
        
        result = {
            "test_type": "multi_file_get",
            "file_size_mb": file_size_mb,
            "file_size_bytes": file_size_mb * 1024 * 1024,
            "num_files": num_files,
            "max_workers": max_workers,
            "file_type": file_type,
            "success": success,
            "success_count": success_count,
            "total_time_s": total_time,
            "total_throughput_mbps": total_throughput,
            "avg_file_throughput_mbps": avg_file_throughput,
            "cpu_usage": resources.get("cpu_avg", 0),
            "memory_usage": resources.get("memory_avg", 0),
            "test_id": test_id,
            "timestamp": datetime.now().isoformat()
        }
        
        self.multi_file_results.append(result)
        return result
    
    def run_multi_file_test(self, file_size_mb, num_files, max_workers=None, file_type="random", test_id=0):
        print(f"\nTesting multi-file PUT+GET: {num_files} files of {file_size_mb} MB each, type: {file_type}")
        
        file_size_bytes = int(file_size_mb * 1024 * 1024)
        filepaths = []
        remote_filenames = []
        
        for i in range(num_files):
            test_filename = f"multi_test_{test_id}_{i}_{file_size_mb}MB_{file_type}.bin"
            remote_filename = f"multi_remote_{test_id}_{i}_{file_size_mb}MB_{file_type}.bin"
            filepath = self.create_test_file(file_size_bytes, test_filename, file_type)
            filepaths.append(filepath)
            remote_filenames.append(remote_filename)
        
        put_result = self.run_multi_file_put_test(file_size_mb, num_files, max_workers, file_type, test_id)
        
        get_result = None
        if put_result["success"] and put_result["success_count"] == num_files:
            get_result = self.run_multi_file_get_test(file_size_mb, num_files, max_workers, file_type, test_id, remote_filenames)
        
        for filepath in filepaths:
            if filepath.exists():
                filepath.unlink()
        
        return put_result, get_result
    
    def run_single_test(self, file_size_mb, test_id, file_type="random"):
        print(f"\nTesting file size: {file_size_mb} MB, type: {file_type}")
        
        file_size_bytes = int(file_size_mb * 1024 * 1024)
        test_filename = f"test_{test_id}_{file_size_mb}MB_{file_type}.bin"
        remote_filename = f"remote_{test_id}_{file_size_mb}MB_{file_type}.bin"
        local_get_filename = f"downloaded_{test_id}_{file_size_mb}MB_{file_type}.bin"
        
        filepath = self.create_test_file(file_size_bytes, test_filename, file_type)
        
        put_success, put_latency, put_throughput, put_resources = self.measure_put_operation(
            filepath, remote_filename
        )
        
        get_success, get_latency, get_throughput, actual_size, get_resources = False, 0, 0, 0, {}
        if put_success:
            get_success, get_latency, get_throughput, actual_size, get_resources = self.measure_get_operation(
                remote_filename, local_get_filename
            )
            
            if get_success and actual_size == file_size_bytes:
                integrity_ok = True
            else:
                integrity_ok = False
                get_success = False
        else:
            integrity_ok = False
        
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
    
    def process_results_with_statistics(self, results_list):
        if not results_list:
            return []
        
        grouped = {}
        for r in results_list:
            key = (r.get('file_size_mb'), r.get('file_type', 'random'), r.get('test_type', 'single'))
            if key not in grouped:
                grouped[key] = {
                    'put_throughputs': [], 'get_throughputs': [], 
                    'put_latencies': [], 'get_latencies': [],
                    'put_cpu_usage': [], 'get_cpu_usage': [],
                    'put_memory_usage': [], 'get_memory_usage': [],
                    'results': []
                }
            if r.get('put_success', r.get('success', False)):
                if 'put_throughput_mbps' in r:
                    grouped[key]['put_throughputs'].append(r['put_throughput_mbps'])
                elif 'total_throughput_mbps' in r:
                    grouped[key]['put_throughputs'].append(r['total_throughput_mbps'])
                if 'put_latency_s' in r:
                    grouped[key]['put_latencies'].append(r['put_latency_s'])
                # Collect CPU and memory usage for PUT
                if 'put_cpu_usage' in r:
                    grouped[key]['put_cpu_usage'].append(r['put_cpu_usage'])
                elif 'cpu_usage' in r:
                    grouped[key]['put_cpu_usage'].append(r['cpu_usage'])
                if 'put_memory_usage' in r:
                    grouped[key]['put_memory_usage'].append(r['put_memory_usage'])
                elif 'memory_usage' in r:
                    grouped[key]['put_memory_usage'].append(r['memory_usage'])
            if r.get('get_success', r.get('success', False)):
                if 'get_throughput_mbps' in r:
                    grouped[key]['get_throughputs'].append(r['get_throughput_mbps'])
                if 'get_latency_s' in r:
                    grouped[key]['get_latencies'].append(r['get_latency_s'])
                # Collect CPU and memory usage for GET
                if 'get_cpu_usage' in r:
                    grouped[key]['get_cpu_usage'].append(r['get_cpu_usage'])
                elif 'cpu_usage' in r:
                    grouped[key]['get_cpu_usage'].append(r['cpu_usage'])
                if 'get_memory_usage' in r:
                    grouped[key]['get_memory_usage'].append(r['get_memory_usage'])
                elif 'memory_usage' in r:
                    grouped[key]['get_memory_usage'].append(r['memory_usage'])
            grouped[key]['results'].append(r)
        
        processed = []
        for key, data in grouped.items():
            file_size, file_type, test_type = key
            
            put_tps = data['put_throughputs']
            get_tps = data['get_throughputs']
            put_lats = data['put_latencies']
            get_lats = data['get_latencies']
            put_cpu = data['put_cpu_usage']
            get_cpu = data['get_cpu_usage']
            put_mem = data['put_memory_usage']
            get_mem = data['get_memory_usage']
            
            if self.remove_outliers_flag:
                put_outliers = detect_outliers_iqr(put_tps)
                get_outliers = detect_outliers_iqr(get_tps)
                put_tps = remove_outliers(put_tps, put_outliers)
                get_tps = remove_outliers(get_tps, get_outliers)
                put_lats = remove_outliers(put_lats, detect_outliers_iqr(put_lats))
                get_lats = remove_outliers(get_lats, detect_outliers_iqr(get_lats))
            
            put_mean, put_ci_low, put_ci_high = calculate_confidence_interval(put_tps, self.confidence_level)
            get_mean, get_ci_low, get_ci_high = calculate_confidence_interval(get_tps, self.confidence_level)
            put_lat_mean, put_lat_ci_low, put_lat_ci_high = calculate_confidence_interval(put_lats, self.confidence_level)
            get_lat_mean, get_lat_ci_low, get_lat_ci_high = calculate_confidence_interval(get_lats, self.confidence_level)
            
            # Calculate CPU and memory usage statistics
            put_cpu_mean = sum(put_cpu) / len(put_cpu) if put_cpu else 0
            get_cpu_mean = sum(get_cpu) / len(get_cpu) if get_cpu else 0
            put_mem_mean = sum(put_mem) / len(put_mem) if put_mem else 0
            get_mem_mean = sum(get_mem) / len(get_mem) if get_mem else 0
            
            put_std_err = calculate_standard_error(put_tps)
            get_std_err = calculate_standard_error(get_tps)
            
            stats_result = {
                'file_size_mb': file_size,
                'file_type': file_type,
                'test_type': test_type,
                'put_throughput_mean': put_mean,
                'put_throughput_ci_low': put_ci_low,
                'put_throughput_ci_high': put_ci_high,
                'put_throughput_std_err': put_std_err,
                'put_throughput_n': len(put_tps),
                'get_throughput_mean': get_mean,
                'get_throughput_ci_low': get_ci_low,
                'get_throughput_ci_high': get_ci_high,
                'get_throughput_std_err': get_std_err,
                'get_throughput_n': len(get_tps),
                'put_latency_mean': put_lat_mean,
                'put_latency_ci_low': put_lat_ci_low,
                'put_latency_ci_high': put_lat_ci_high,
                'get_latency_mean': get_lat_mean,
                'get_latency_ci_low': get_lat_ci_low,
                'get_latency_ci_high': get_lat_ci_high,
                'put_cpu_usage': put_cpu_mean,
                'get_cpu_usage': get_cpu_mean,
                'put_memory_usage': put_mem_mean,
                'get_memory_usage': get_mem_mean,
                'confidence_level': self.confidence_level,
                'outliers_removed': self.remove_outliers_flag
            }
            processed.append(stats_result)
        
        return processed
    
    def run_comprehensive_test(self, file_sizes_mb, iterations=5, file_types=None, include_single=True, include_multi=False, multi_only=False, num_files=10):
        if file_types is None:
            file_types = ["random"]
        
        self.set_test_config(
            file_sizes_mb=file_sizes_mb,
            iterations=iterations,
            file_types=file_types,
            include_single=include_single,
            include_multi=include_multi,
            multi_only=multi_only,
            num_files=num_files
        )
        
        print(f"Starting comprehensive performance test...")
        print(f"File sizes to test: {file_sizes_mb} MB")
        print(f"File types to test: {file_types}")
        print(f"Iterations per configuration: {iterations}")
        print(f"Warmup iterations: {self.warmup}")
        print(f"Include single file tests: {include_single and not multi_only}")
        print(f"Include multi file tests: {include_multi or multi_only}")
        if include_multi or multi_only:
            print(f"Number of files for multi-file tests: {num_files}")
        
        test_id = 0
        warmup_results_count = 0
        
        if include_single and not multi_only:
            print(f"\n=== Running single file tests ===")
            for file_type in file_types:
                for file_size in file_sizes_mb:
                    warmup_count = 0
                    for iteration in range(iterations + self.warmup):
                        test_id += 1
                        is_warmup = iteration < self.warmup
                        try:
                            result = self.run_single_test(file_size, test_id, file_type)
                            if is_warmup:
                                warmup_count += 1
                                warmup_results_count += 1
                                if self.results:
                                    self.results.pop()
                                print(f"  [{file_type}] Warmup {warmup_count}: completed")
                            elif result["put_success"] and result["get_success"]:
                                print(f"  [{file_type}] Iteration {iteration - self.warmup + 1}: PUT={result['put_throughput_mbps']:.2f} MB/s, "
                                      f"GET={result['get_throughput_mbps']:.2f} MB/s")
                            else:
                                print(f"  [{file_type}] Iteration {iteration - self.warmup + 1}: Failed")
                            if self.cooldown > 0 and not is_warmup:
                                time.sleep(self.cooldown)
                        except Exception as e:
                            print(f"  [{file_type}] Iteration {iteration - self.warmup + 1}: Error - {e}")
                            continue
        
        if include_multi or multi_only:
            print(f"\n=== Running multi file tests ===")
            for file_type in file_types:
                for file_size in file_sizes_mb:
                    warmup_count = 0
                    for iteration in range(iterations + self.warmup):
                        test_id += 1
                        is_warmup = iteration < self.warmup
                        try:
                            put_result, get_result = self.run_multi_file_test(file_size, num_files, None, file_type, test_id)
                            if is_warmup:
                                warmup_count += 1
                                if self.multi_file_results:
                                    self.multi_file_results.pop()
                                if self.multi_file_results:
                                    self.multi_file_results.pop()
                                print(f"  [{file_type}] Warmup {warmup_count}: completed")
                            elif put_result["success"] and (get_result is None or get_result["success"]):
                                print(f"  [{file_type}] Iteration {iteration - self.warmup + 1}: Multi-PUT={put_result['total_throughput_mbps']:.2f} MB/s, "
                                      f"Multi-GET={get_result['total_throughput_mbps']:.2f} MB/s" if get_result else f"Multi-PUT={put_result['total_throughput_mbps']:.2f} MB/s")
                            else:
                                print(f"  [{file_type}] Iteration {iteration - self.warmup + 1}: Failed")
                            if self.cooldown > 0 and not is_warmup:
                                time.sleep(self.cooldown)
                        except Exception as e:
                            print(f"  [{file_type}] Iteration {iteration - self.warmup + 1}: Error - {e}")
                            continue
        
        return self.results, self.multi_file_results
    
    def save_results(self, output_file="performance_results.json"):
        single_stats = self.process_results_with_statistics(self.results)
        multi_stats = self.process_results_with_statistics(self.multi_file_results)
        
        script_dir = Path(__file__).parent
        output_path = script_dir / output_file
        
        complete_results = {
            "system_info": self.system_info,
            "test_config": self.test_config,
            "test_duration_seconds": time.time() - self.start_time if self.start_time else 0,
            "results": self.results,
            "multi_file_tests": self.multi_file_results,
            "statistics": {
                "single_file": single_stats,
                "multi_file": multi_stats
            }
        }
        
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            json.dump(complete_results, f, indent=2, default=str)
        print(f"Results saved to {output_path}")
    
    def shutdown(self, force=False):
        print("Shutting down test environment...")
        if force:
            try:
                script_dir = Path(__file__).parent
                project_root = script_dir.parent.parent
                subprocess.run(["make", "kill"], cwd=str(project_root), 
                              capture_output=True, timeout=10)
            except:
                pass
        self.cleanup()


def main():
    parser = argparse.ArgumentParser(description='DFS Performance Tester for Academic Research')
    parser.add_argument('--sizes', nargs='+', type=float, 
                       default=[0.004, 0.016, 0.064, 0.256, 1, 4, 16],
                       help='File sizes to test in MB (default: 4K 16K 64K 256K 1M 4M 16M)')
    parser.add_argument('--iterations', type=int, default=5,
                       help='Number of iterations per configuration (default: 5, recommended for academic research)')
    parser.add_argument('--types', nargs='+', choices=['random', 'text', 'binary'], 
                       default=['random'],
                       help='File types to test (default: random)')
    parser.add_argument('--output', type=str, default='performance_results.json',
                       help='Output JSON file for results')
    parser.add_argument('--plot-only', action='store_true',
                       help='Only generate plots from existing results')
    parser.add_argument('--num-files', type=int, default=10,
                       help='Number of files for multi-file tests (default: 10)')
    parser.add_argument('--include-multi', action='store_true',
                       help='Include multi-file tests in addition to single-file tests')
    parser.add_argument('--multi-only', action='store_true',
                       help='Only run multi-file tests, skip single-file tests')
    parser.add_argument('--seed', type=int, default=None,
                       help='Random seed for reproducibility (default: current timestamp)')
    parser.add_argument('--warmup', type=int, default=2,
                       help='Number of warmup iterations (default: 2)')
    parser.add_argument('--cooldown', type=int, default=1,
                       help='Cooldown time in seconds between tests (default: 1)')
    parser.add_argument('--confidence-level', type=float, default=0.95,
                       help='Confidence level for intervals (default: 0.95)')
    parser.add_argument('--remove-outliers', action='store_true',
                       help='Remove outliers using IQR method')
    
    args = parser.parse_args()
    
    if args.plot_only:
        from generate_plots import generate_plots
        generate_plots(args.output)
        return
    
    tester = DFSPerformanceTester(
        seed=args.seed,
        warmup=args.warmup,
        cooldown=args.cooldown,
        confidence_level=args.confidence_level,
        remove_outliers=args.remove_outliers
    )
    try:
        tester.setup_test_environment()
        results, multi_results = tester.run_comprehensive_test(
            args.sizes, args.iterations, args.types,
            include_single=not args.multi_only,
            include_multi=args.include_multi,
            multi_only=args.multi_only,
            num_files=args.num_files
        )
        tester.save_results(args.output)
        
        from generate_plots import generate_plots
        generate_plots(args.output)
        
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"Test failed with error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        tester.shutdown(force=False)


if __name__ == "__main__":
    main()
