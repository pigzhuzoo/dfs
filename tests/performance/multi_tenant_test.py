#!/usr/bin/env python3
"""
Multi-Tenant Throughput Test for DFS

This script tests the DFS system under multi-tenant conditions,
measuring latency distribution across 1 to N tenants, and availability
under different connection counts.
"""

import os
import sys
import json
import time
import argparse
import subprocess
import threading
import tempfile
from dataclasses import dataclass, asdict, field
from typing import List, Dict, Optional
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
import statistics
from pathlib import Path


@dataclass
class TenantConfig:
    username: str
    password: str
    num_connections: int = 1
    num_files: int = 5
    file_size_mb: float = 1.0


@dataclass
class OperationResult:
    success: bool
    latency_ms: float
    error: str = ""


@dataclass
class TenantLatencyResult:
    tenant_count: int
    p50_latency_ms: float = 0.0
    p95_latency_ms: float = 0.0
    p99_latency_ms: float = 0.0
    latencies: List[float] = field(default_factory=list)
    successful_ops: int = 0
    failed_ops: int = 0


@dataclass
class AvailabilityResult:
    max_connections: int
    acceptable_latency_ms: float = 1000.0
    success_rate: float = 0.0
    avg_latency_ms: float = 0.0


class MultiTenantTester:
    def __init__(self, dfc_path: str, config_path: str, test_dir: str):
        self.dfc_path = dfc_path
        self.config_path = config_path
        self.test_dir = test_dir
        self.results_lock = threading.Lock()
        self.config_cache = {}  # 缓存租户配置文件路径
        # 使用服务器已有的两个用户
        self.available_users = [
            ("Bob", "ComplextPassword"),
            ("Alice", "SimplePassword")
        ]
        # 限制最大并发工作线程数
        self.max_workers = min(4, os.cpu_count() or 2)
        
    def generate_test_file(self, size_mb: float, filepath: str):
        size_bytes = int(size_mb * 1024 * 1024)
        with open(filepath, 'wb') as f:
            f.write(os.urandom(size_bytes))
        return filepath
    
    def _get_tenant_credentials(self, tenant_idx: int):
        """根据租户索引获取循环使用的凭据"""
        return self.available_users[tenant_idx % len(self.available_users)]
    
    def _create_tenant_config(self, username: str, password: str) -> str:
        """为租户创建临时配置文件"""
        cache_key = f"{username}_{password}"
        if cache_key in self.config_cache:
            return self.config_cache[cache_key]
        
        temp_config_path = os.path.join(self.test_dir, f"config_{username}.conf")
        
        if not os.path.exists(temp_config_path):
            with open(self.config_path, 'r') as src_f:
                content = src_f.read()
            
            with open(temp_config_path, 'w') as dst_f:
                for line in content.splitlines():
                    if line.startswith('Username:'):
                        dst_f.write(f"Username: {username}\n")
                    elif line.startswith('Password:'):
                        dst_f.write(f"Password: {password}\n")
                    else:
                        dst_f.write(f"{line}\n")
        
        self.config_cache[cache_key] = temp_config_path
        return temp_config_path
    
    def run_put_operation(self, username: str, password: str, 
                          local_file: str, remote_file: str) -> OperationResult:
        start_time = time.time()
        
        try:
            tenant_config = self._create_tenant_config(username, password)
            cmd_str = f"PUT {local_file} {remote_file}\nEXIT\n"
            
            result = subprocess.run(
                [self.dfc_path, tenant_config],
                input=cmd_str,
                capture_output=True,
                text=True,
                timeout=120
            )
            
            end_time = time.time()
            latency_ms = (end_time - start_time) * 1000
            
            success = result.returncode == 0
            
            return OperationResult(
                success=success,
                latency_ms=latency_ms,
                error=result.stderr if not success else ""
            )
            
        except subprocess.TimeoutExpired:
            return OperationResult(success=False, latency_ms=120000, error="Timeout")
        except Exception as e:
            return OperationResult(success=False, latency_ms=0, error=str(e))
    
    def run_tenant_latency_test(self, tenant_count: int, iterations: int, 
                                 file_size_mb: float) -> TenantLatencyResult:
        result = TenantLatencyResult(tenant_count=tenant_count)
        
        os.makedirs(self.test_dir, exist_ok=True)
        
        tenants = []
        for i in range(tenant_count):
            username, password = self._get_tenant_credentials(i)
            tenants.append(TenantConfig(
                username=username,
                password=password,
                num_connections=1,
                num_files=iterations,
                file_size_mb=file_size_mb
            ))
        
        all_latencies = []
        completed_tenants = 0
        
        def run_single_tenant(tenant: TenantConfig, tenant_idx: int):
            tenant_latencies = []
            for i in range(iterations):
                local_file = os.path.join(self.test_dir, f"tenant_{tenant_idx}_test_{i}.bin")
                remote_file = f"tenant_{tenant_idx}_test_{i}.bin"
                
                self.generate_test_file(file_size_mb, local_file)
                
                op_result = self.run_put_operation(
                    tenant.username, tenant.password,
                    local_file, remote_file
                )
                
                if op_result.success:
                    with self.results_lock:
                        result.successful_ops += 1
                        all_latencies.append(op_result.latency_ms)
                        tenant_latencies.append(op_result.latency_ms)
                else:
                    with self.results_lock:
                        result.failed_ops += 1
                        print(f"    Warning: Tenant {tenant_idx} ({tenant.username}) iteration {i} failed: {op_result.error}")
                
                if os.path.exists(local_file):
                    os.remove(local_file)
            
            nonlocal completed_tenants
            with self.results_lock:
                completed_tenants += 1
                print(f"    Tenant {tenant_idx} ({tenant.username}) completed ({completed_tenants}/{tenant_count})")
            
            return tenant_latencies
        
        with ThreadPoolExecutor(max_workers=min(tenant_count, self.max_workers)) as executor:
            futures = [executor.submit(run_single_tenant, tenant, i) for i, tenant in enumerate(tenants)]
            for future in as_completed(futures):
                try:
                    future.result()
                except Exception as e:
                    print(f"    Tenant error: {e}")
        
        if all_latencies:
            all_latencies.sort()
            result.p50_latency_ms = all_latencies[len(all_latencies) * 50 // 100]
            result.p95_latency_ms = all_latencies[len(all_latencies) * 95 // 100]
            result.p99_latency_ms = all_latencies[min(len(all_latencies) * 99 // 100, len(all_latencies) - 1)]
            result.latencies = all_latencies
        
        return result
    
    def run_availability_test(self, max_connections: int, acceptable_latency_ms: float,
                              file_size_mb: float = 0.064) -> List[AvailabilityResult]:
        results = []
        
        os.makedirs(self.test_dir, exist_ok=True)
        
        # 使用已有的第一个用户进行可用性测试
        username, password = self.available_users[0]
        
        local_file = os.path.join(self.test_dir, "availability_test.bin")
        remote_file = "availability_test.bin"
        self.generate_test_file(file_size_mb, local_file)
        
        for conn_count in range(1, max_connections + 1):
            print(f"  Testing {conn_count} connections...")
            successful = 0
            total = 0
            latencies = []
            
            def run_single_connection(conn_id: int):
                nonlocal successful, total, latencies
                # 减少每个连接的操作次数
                for i in range(2):
                    op_result = self.run_put_operation(
                        username, password,
                        local_file, f"{remote_file}_{conn_count}_{conn_id}_{i}"
                    )
                    with self.results_lock:
                        total += 1
                        if op_result.success:
                            successful += 1
                            latencies.append(op_result.latency_ms)
            
            with ThreadPoolExecutor(max_workers=min(conn_count, self.max_workers)) as executor:
                futures = [executor.submit(run_single_connection, i) for i in range(conn_count)]
                for future in as_completed(futures):
                    try:
                        future.result()
                    except Exception as e:
                        print(f"    Warning: Connection error: {e}")
            
            result = AvailabilityResult(
                max_connections=conn_count,
                acceptable_latency_ms=acceptable_latency_ms
            )
            
            if total > 0:
                result.success_rate = successful / total * 100
            if latencies:
                result.avg_latency_ms = statistics.mean(latencies)
            
            results.append(result)
            
            # 测试完一个连接数后也冷却一下
            time.sleep(0.5)
        
        if os.path.exists(local_file):
            os.remove(local_file)
        
        return results


def generate_plots(latency_results: List[TenantLatencyResult], 
                   availability_results: Optional[List[AvailabilityResult]],
                   output_dir: str):
    try:
        import matplotlib.pyplot as plt
        import numpy as np
        
        multi_tenant_dir = os.path.join(output_dir, 'multi_tenant')
        os.makedirs(multi_tenant_dir, exist_ok=True)
        
        tenant_counts = [r.tenant_count for r in latency_results]
        p50 = [r.p50_latency_ms for r in latency_results]
        p95 = [r.p95_latency_ms for r in latency_results]
        p99 = [r.p99_latency_ms for r in latency_results]
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        x = np.arange(len(tenant_counts))
        width = 0.25
        
        ax.bar(x - width, p50, width, label='P50', color='steelblue')
        ax.bar(x, p95, width, label='P95', color='orange')
        ax.bar(x + width, p99, width, label='P99', color='red')
        
        ax.set_xlabel('Number of Tenants')
        ax.set_ylabel('Latency (ms)')
        ax.set_title('Latency Distribution vs Number of Tenants')
        ax.set_xticks(x)
        ax.set_xticklabels([f'{c} Tenant{"s" if c > 1 else ""}' for c in tenant_counts])
        ax.legend()
        ax.grid(True, alpha=0.3, axis='y')
        
        plt.tight_layout()
        plt.savefig(os.path.join(multi_tenant_dir, 'latency_vs_tenants.png'), dpi=150)
        plt.savefig(os.path.join(multi_tenant_dir, 'latency_vs_tenants.pdf'), dpi=150)
        plt.close()
        
        if availability_results:
            fig, ax = plt.subplots(figsize=(10, 6))
            
            connections = [r.max_connections for r in availability_results]
            success_rates = [r.success_rate for r in availability_results]
            avg_latencies = [r.avg_latency_ms for r in availability_results]
            acceptable_latency = availability_results[0].acceptable_latency_ms if availability_results else 1000.0
            
            ax2 = ax.twinx()
            
            bars = ax.bar(connections, success_rates, color='steelblue', alpha=0.7, label='Success Rate')
            line = ax2.plot(connections, avg_latencies, 'o-', color='orange', linewidth=2, markersize=8, label='Avg Latency')
            
            ax2.axhline(y=acceptable_latency, color='red', linestyle='--', linewidth=1.5, label=f'Acceptable: {acceptable_latency}ms')
            
            ax.set_xlabel('Number of Concurrent Connections')
            ax.set_ylabel('Success Rate (%)', color='steelblue')
            ax2.set_ylabel('Average Latency (ms)', color='orange')
            ax.set_title(f'Availability Test: Connection Count vs Latency')
            ax.set_ylim([0, 105])
            ax.grid(True, alpha=0.3, axis='y')
            ax.set_xticks(connections)
            ax.set_xticklabels([str(c) for c in connections])
            
            lines1, labels1 = ax.get_legend_handles_labels()
            lines2, labels2 = ax2.get_legend_handles_labels()
            ax.legend(lines1 + lines2, labels1 + labels2, loc='best')
            
            plt.tight_layout()
            plt.savefig(os.path.join(multi_tenant_dir, 'availability_test.png'), dpi=150)
            plt.savefig(os.path.join(multi_tenant_dir, 'availability_test.pdf'), dpi=150)
            plt.close()
        
        print(f"Plots saved to {multi_tenant_dir}/")
        
    except ImportError:
        print("matplotlib not available, skipping plots")


def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    parser = argparse.ArgumentParser(description='Multi-Tenant Latency and Availability Test for DFS')
    parser.add_argument('--dfc', default=str(project_root / 'bin' / 'dfc'), help='Path to dfc binary')
    parser.add_argument('--config', default=str(project_root / 'conf' / 'dfc.conf'), help='Path to dfc.conf')
    parser.add_argument('--output', default='plots/multi_tenant_results.json', help='Output file')
    parser.add_argument('--max-tenants', type=int, default=2, help='Maximum number of tenants to test')
    parser.add_argument('--iterations', type=int, default=1, help='Iterations per tenant')
    parser.add_argument('--file-size', type=float, default=0.064, help='File size in MB')
    parser.add_argument('--acceptable-latency', type=float, default=3000.0, 
                       help='Acceptable latency in ms for availability test')
    parser.add_argument('--max-connections', type=int, default=5, 
                       help='Maximum connections for availability test')
    parser.add_argument('--plots', action='store_true', help='Generate plots')
    parser.add_argument('--skip-availability', action='store_true', help='Skip availability test')
    parser.add_argument('--cooldown', type=float, default=1.0, 
                       help='Cooldown seconds between tenant tests')
    
    args = parser.parse_args()
    
    test_dir = tempfile.mkdtemp(prefix='dfs_multi_tenant_')
    
    tester = MultiTenantTester(
        dfc_path=args.dfc,
        config_path=args.config,
        test_dir=test_dir
    )
    
    all_results = {}
    latency_results = []
    
    print(f"\n{'='*70}")
    print("Running Latency Test: 1 to N Tenants")
    print('='*70)
    print(f"Parameters: File size = {args.file_size} MB, Iterations = {args.iterations}")
    
    try:
        for tenant_count in range(1, args.max_tenants + 1):
            print(f"\nTesting with {tenant_count} tenant{'s' if tenant_count > 1 else ''}...")
            result = tester.run_tenant_latency_test(
                tenant_count=tenant_count,
                iterations=args.iterations,
                file_size_mb=args.file_size
            )
            latency_results.append(result)
            print(f"  P50: {result.p50_latency_ms:.1f}ms, P95: {result.p95_latency_ms:.1f}ms, "
                  f"P99: {result.p99_latency_ms:.1f}ms")
            print(f"  Success: {result.successful_ops}, Failed: {result.failed_ops}")
            
            if tenant_count < args.max_tenants:
                print(f"  Cooling down for {args.cooldown} seconds...")
                time.sleep(args.cooldown)
        
        all_results['latency_tests'] = [asdict(r) for r in latency_results]
        
        availability_results = None
        if not args.skip_availability:
            print(f"\n{'='*70}")
            print("Running Availability Test")
            print('='*70)
            
            availability_results = tester.run_availability_test(
                max_connections=args.max_connections,
                acceptable_latency_ms=args.acceptable_latency,
                file_size_mb=0.064
            )
            all_results['availability_tests'] = [asdict(r) for r in availability_results]
            
            print(f"\nAvailability Test Results:")
            print('-' * 70)
            for result in availability_results:
                print(f"  Connections: {result.max_connections:2d} | "
                      f"Success: {result.success_rate:5.1f}% | "
                      f"Latency: {result.avg_latency_ms:7.1f}ms")
        
        with open(args.output, 'w') as f:
            json.dump(all_results, f, indent=2)
        
        print(f"\nResults saved to {args.output}")
        
        if args.plots:
            output_dir = os.path.dirname(args.output) or '.'
            generate_plots(latency_results, availability_results, output_dir)
    finally:
        import shutil
        shutil.rmtree(test_dir, ignore_errors=True)
    
    print(f"\n{'='*70}")
    print("Summary")
    print('='*70)
    for result in latency_results:
        print(f"{result.tenant_count} Tenant{'s' if result.tenant_count > 1 else ''}: "
              f"P50={result.p50_latency_ms:.1f}ms, P95={result.p95_latency_ms:.1f}ms, "
              f"P99={result.p99_latency_ms:.1f}ms")


if __name__ == '__main__':
    main()
