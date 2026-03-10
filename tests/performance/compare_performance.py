#!/usr/bin/env python3
"""
Performance Comparison Script for DFS
Compares CPU-based encryption vs FPGA-accelerated encryption
"""

import json
import sys
import os
from datetime import datetime
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # Use non-interactive backend
import numpy as np

def load_results(filepath):
    """Load performance results from JSON file"""
    with open(filepath, 'r') as f:
        return json.load(f)

def compare_results(cpu_results, fpga_results):
    """Compare CPU and FPGA performance results"""
    print("\n" + "="*80)
    print("           DFS Performance Comparison: CPU vs FPGA")
    print("="*80)
    
    cpu_stats = cpu_results.get('statistics', {}).get('single_file', [])
    fpga_stats = fpga_results.get('statistics', {}).get('single_file', [])
    
    if not cpu_stats or not fpga_stats:
        print("Error: Missing statistics in results files")
        return
    
    print(f"\n{'File Size':<12} {'CPU PUT (MB/s)':<18} {'FPGA PUT (MB/s)':<18} {'Speedup':<10} {'CPU GET (MB/s)':<18} {'FPGA GET (MB/s)':<18} {'Speedup':<10}")
    print("-" * 120)
    
    total_put_speedup = 0
    total_get_speedup = 0
    count = 0
    
    for cpu, fpga in zip(cpu_stats, fpga_stats):
        size = cpu['file_size_mb']
        
        cpu_put = cpu['put_throughput_mean']
        fpga_put = fpga['put_throughput_mean']
        put_speedup = fpga_put / cpu_put if cpu_put > 0 else 0
        
        cpu_get = cpu['get_throughput_mean']
        fpga_get = fpga['get_throughput_mean']
        get_speedup = fpga_get / cpu_get if cpu_get > 0 else 0
        
        total_put_speedup += put_speedup
        total_get_speedup += get_speedup
        count += 1
        
        print(f"{size:<12.3f} {cpu_put:<18.2f} {fpga_put:<18.2f} {put_speedup:<10.2f}x {cpu_get:<18.2f} {fpga_get:<18.2f} {get_speedup:<10.2f}x")
    
    print("-" * 120)
    if count > 0:
        avg_put_speedup = total_put_speedup / count
        avg_get_speedup = total_get_speedup / count
        print(f"{'Average':<12} {'':<18} {'':<18} {avg_put_speedup:<10.2f}x {'':<18} {'':<18} {avg_get_speedup:<10.2f}x")
    
    print("\n" + "="*80)
    print("                           Latency Comparison")
    print("="*80)
    
    print(f"\n{'File Size':<12} {'CPU PUT (ms)':<15} {'FPGA PUT (ms)':<15} {'Reduction':<12} {'CPU GET (ms)':<15} {'FPGA GET (ms)':<15} {'Reduction':<12}")
    print("-" * 110)
    
    for cpu, fpga in zip(cpu_stats, fpga_stats):
        size = cpu['file_size_mb']
        
        cpu_put_lat = cpu['put_latency_mean'] * 1000  # Convert to ms
        fpga_put_lat = fpga['put_latency_mean'] * 1000
        put_reduction = (1 - fpga_put_lat / cpu_put_lat) * 100 if cpu_put_lat > 0 else 0
        
        cpu_get_lat = cpu['get_latency_mean'] * 1000
        fpga_get_lat = fpga['get_latency_mean'] * 1000
        get_reduction = (1 - fpga_get_lat / cpu_get_lat) * 100 if cpu_get_lat > 0 else 0
        
        print(f"{size:<12.3f} {cpu_put_lat:<15.2f} {fpga_put_lat:<15.2f} {put_reduction:<11.1f}% {cpu_get_lat:<15.2f} {fpga_get_lat:<15.2f} {get_reduction:<11.1f}%")
    
    print("\n" + "="*80)
    print("                           System Information")
    print("="*80)
    
    cpu_sys = cpu_results.get('system_info', {})
    fpga_sys = fpga_results.get('system_info', {})
    
    print(f"\nCPU Test:")
    print(f"  Timestamp: {cpu_sys.get('timestamp', 'N/A')}")
    print(f"  CPU Count: {cpu_sys.get('cpu_count', 'N/A')}")
    print(f"  Memory: {cpu_sys.get('memory_total_gb', 'N/A'):.1f} GB")
    
    print(f"\nFPGA Test:")
    print(f"  Timestamp: {fpga_sys.get('timestamp', 'N/A')}")
    print(f"  CPU Count: {fpga_sys.get('cpu_count', 'N/A')}")
    print(f"  Memory: {fpga_sys.get('memory_total_gb', 'N/A'):.1f} GB")
    
    print("\n" + "="*80)

def format_file_size(size_mb):
    """Format file size for display"""
    if size_mb < 1:
        return f'{int(size_mb * 1024)}KB'
    else:
        return f'{int(size_mb)}MB'

def save_figure(fig, output_dir, filename, formats=['pdf', 'png']):
    """Save figure in multiple formats"""
    for fmt in formats:
        filepath = f'{output_dir}/{filename}.{fmt}'
        fig.savefig(filepath, format=fmt, dpi=300, bbox_inches='tight')
    plt.close(fig)

def generate_comparison_plots(cpu_stats, fpga_stats, output_dir):
    """Generate comparison plots in various subdirectories"""
    
    # Extract data
    sizes = [s['file_size_mb'] for s in cpu_stats]
    size_labels = [format_file_size(s) for s in sizes]
    
    cpu_put = [s['put_throughput_mean'] for s in cpu_stats]
    fpga_put = [s['put_throughput_mean'] for s in fpga_stats]
    cpu_get = [s['get_throughput_mean'] for s in cpu_stats]
    fpga_get = [s['get_throughput_mean'] for s in fpga_stats]
    
    cpu_put_lat = [s['put_latency_mean'] * 1000 for s in cpu_stats]
    fpga_put_lat = [s['put_latency_mean'] * 1000 for s in fpga_stats]
    cpu_get_lat = [s['get_latency_mean'] * 1000 for s in cpu_stats]
    fpga_get_lat = [s['get_latency_mean'] * 1000 for s in fpga_stats]
    
    cpu_put_std = [s.get('put_throughput_std', 0) for s in cpu_stats]
    fpga_put_std = [s.get('put_throughput_std', 0) for s in fpga_stats]
    cpu_get_std = [s.get('get_throughput_std', 0) for s in cpu_stats]
    fpga_get_std = [s.get('get_throughput_std', 0) for s in fpga_stats]
    
    # Extract CPU and Memory usage data
    cpu_put_cpu_usage = [s.get('put_cpu_usage', s.get('cpu_usage', 0)) for s in cpu_stats]
    fpga_put_cpu_usage = [s.get('put_cpu_usage', s.get('cpu_usage', 0)) for s in fpga_stats]
    cpu_get_cpu_usage = [s.get('get_cpu_usage', s.get('cpu_usage', 0)) for s in cpu_stats]
    fpga_get_cpu_usage = [s.get('get_cpu_usage', s.get('cpu_usage', 0)) for s in fpga_stats]
    
    cpu_put_mem_usage = [s.get('put_memory_usage', s.get('memory_usage', 0)) for s in cpu_stats]
    fpga_put_mem_usage = [s.get('put_memory_usage', s.get('memory_usage', 0)) for s in fpga_stats]
    cpu_get_mem_usage = [s.get('get_memory_usage', s.get('memory_usage', 0)) for s in cpu_stats]
    fpga_get_mem_usage = [s.get('get_memory_usage', s.get('memory_usage', 0)) for s in fpga_stats]
    
    # Calculate speedup
    put_speedup = [fpga_put[i] / cpu_put[i] if cpu_put[i] > 0 else 0 for i in range(len(cpu_put))]
    get_speedup = [fpga_get[i] / cpu_get[i] if cpu_get[i] > 0 else 0 for i in range(len(cpu_get))]
    
    x = np.arange(len(sizes))
    width = 0.35
    
    # ==================== 1. Throughput Comparison Charts ====================
    throughput_dir = os.path.join(output_dir, 'throughput')
    os.makedirs(throughput_dir, exist_ok=True)
    
    # PUT Throughput Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    bars1 = ax.bar(x - width/2, cpu_put, width, label='CPU', color='#3498db', alpha=0.8, yerr=cpu_put_std, capsize=3)
    bars2 = ax.bar(x + width/2, fpga_put, width, label='FPGA', color='#e74c3c', alpha=0.8, yerr=fpga_put_std, capsize=3)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Throughput (MB/s)', fontweight='bold')
    ax.set_title('PUT Throughput: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    save_figure(fig, throughput_dir, 'put_throughput_comparison')
    print(f"PUT throughput comparison saved to: {throughput_dir}/put_throughput_comparison.png")
    
    # GET Throughput Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    bars1 = ax.bar(x - width/2, cpu_get, width, label='CPU', color='#3498db', alpha=0.8, yerr=cpu_get_std, capsize=3)
    bars2 = ax.bar(x + width/2, fpga_get, width, label='FPGA', color='#e74c3c', alpha=0.8, yerr=fpga_get_std, capsize=3)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Throughput (MB/s)', fontweight='bold')
    ax.set_title('GET Throughput: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    save_figure(fig, throughput_dir, 'get_throughput_comparison')
    print(f"GET throughput comparison saved to: {throughput_dir}/get_throughput_comparison.png")
    
    # ==================== 2. Latency Comparison Charts ====================
    latency_dir = os.path.join(output_dir, 'latency')
    os.makedirs(latency_dir, exist_ok=True)
    
    # PUT Latency Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(x, cpu_put_lat, 'o-', label='CPU', color='#3498db', linewidth=2, markersize=8)
    ax.plot(x, fpga_put_lat, 's-', label='FPGA', color='#e74c3c', linewidth=2, markersize=8)
    ax.fill_between(x, cpu_put_lat, alpha=0.2, color='#3498db')
    ax.fill_between(x, fpga_put_lat, alpha=0.2, color='#e74c3c')
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Latency (ms)', fontweight='bold')
    ax.set_title('PUT Latency: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    save_figure(fig, latency_dir, 'put_latency_comparison')
    print(f"PUT latency comparison saved to: {latency_dir}/put_latency_comparison.png")
    
    # GET Latency Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(x, cpu_get_lat, 'o-', label='CPU', color='#3498db', linewidth=2, markersize=8)
    ax.plot(x, fpga_get_lat, 's-', label='FPGA', color='#e74c3c', linewidth=2, markersize=8)
    ax.fill_between(x, cpu_get_lat, alpha=0.2, color='#3498db')
    ax.fill_between(x, fpga_get_lat, alpha=0.2, color='#e74c3c')
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Latency (ms)', fontweight='bold')
    ax.set_title('GET Latency: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    save_figure(fig, latency_dir, 'get_latency_comparison')
    print(f"GET latency comparison saved to: {latency_dir}/get_latency_comparison.png")
    
    # ==================== 3. Speedup Analysis Charts ====================
    speedup_dir = os.path.join(output_dir, 'speedup')
    os.makedirs(speedup_dir, exist_ok=True)
    
    # Speedup Bar Chart
    fig, ax = plt.subplots(figsize=(10, 6))
    bars1 = ax.bar(x - width/2, put_speedup, width, label='PUT Speedup', color='#2ecc71', alpha=0.8)
    bars2 = ax.bar(x + width/2, get_speedup, width, label='GET Speedup', color='#9b59b6', alpha=0.8)
    ax.axhline(y=1, color='red', linestyle='--', alpha=0.5, linewidth=2, label='Baseline (1x)')
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Speedup (FPGA/CPU)', fontweight='bold')
    ax.set_title('FPGA Speedup Analysis', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add value labels on bars
    for bar in bars1:
        height = bar.get_height()
        ax.annotate(f'{height:.2f}x',
                    xy=(bar.get_x() + bar.get_width() / 2, height),
                    xytext=(0, 3),
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=8)
    
    for bar in bars2:
        height = bar.get_height()
        ax.annotate(f'{height:.2f}x',
                    xy=(bar.get_x() + bar.get_width() / 2, height),
                    xytext=(0, 3),
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=8)
    
    plt.tight_layout()
    save_figure(fig, speedup_dir, 'speedup_analysis')
    print(f"Speedup analysis saved to: {speedup_dir}/speedup_analysis.png")
    
    # Speedup Line Chart
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.axhline(y=1, color='red', linestyle='--', alpha=0.5, linewidth=2, label='Baseline (1x)')
    ax.plot(x, put_speedup, 'o-', label='PUT Speedup', color='#2ecc71', linewidth=2, markersize=8)
    ax.plot(x, get_speedup, 's-', label='GET Speedup', color='#9b59b6', linewidth=2, markersize=8)
    ax.fill_between(x, put_speedup, alpha=0.2, color='#2ecc71')
    ax.fill_between(x, get_speedup, alpha=0.2, color='#9b59b6')
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Speedup (FPGA/CPU)', fontweight='bold')
    ax.set_title('FPGA Speedup Trend', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    save_figure(fig, speedup_dir, 'speedup_trend')
    print(f"Speedup trend saved to: {speedup_dir}/speedup_trend.png")
    
    # ==================== 4. Comprehensive Comparison Chart ====================
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('DFS Performance Comparison: CPU vs FPGA', fontsize=16, fontweight='bold')
    
    # Plot 1: PUT Throughput
    ax = axes[0, 0]
    ax.bar(x - width/2, cpu_put, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_put, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Throughput (MB/s)', fontweight='bold')
    ax.set_title('PUT Throughput', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 2: GET Throughput
    ax = axes[0, 1]
    ax.bar(x - width/2, cpu_get, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_get, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Throughput (MB/s)', fontweight='bold')
    ax.set_title('GET Throughput', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 3: PUT Latency
    ax = axes[1, 0]
    ax.plot(x, cpu_put_lat, 'o-', label='CPU', color='#3498db', linewidth=2, markersize=8)
    ax.plot(x, fpga_put_lat, 's-', label='FPGA', color='#e74c3c', linewidth=2, markersize=8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Latency (ms)', fontweight='bold')
    ax.set_title('PUT Latency', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 4: Speedup
    ax = axes[1, 1]
    ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5, label='Baseline (1x)')
    ax.plot(x, put_speedup, 'o-', label='PUT Speedup', color='#2ecc71', linewidth=2, markersize=8)
    ax.plot(x, get_speedup, 's-', label='GET Speedup', color='#9b59b6', linewidth=2, markersize=8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Speedup (FPGA/CPU)', fontweight='bold')
    ax.set_title('FPGA Speedup', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'cpu_vs_fpga_comparison')
    print(f"\nComprehensive comparison saved to: {output_dir}/cpu_vs_fpga_comparison.png")
    
    # ==================== 5. CPU Usage Comparison Charts ====================
    cpu_usage_dir = os.path.join(output_dir, 'cpu_usage')
    os.makedirs(cpu_usage_dir, exist_ok=True)
    
    # PUT CPU Usage Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(x - width/2, cpu_put_cpu_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_put_cpu_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('CPU Usage (%)', fontweight='bold')
    ax.set_title('PUT CPU Usage: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    save_figure(fig, cpu_usage_dir, 'put_cpu_usage_comparison')
    print(f"PUT CPU usage comparison saved to: {cpu_usage_dir}/put_cpu_usage_comparison.png")
    
    # GET CPU Usage Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(x - width/2, cpu_get_cpu_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_get_cpu_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('CPU Usage (%)', fontweight='bold')
    ax.set_title('GET CPU Usage: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    save_figure(fig, cpu_usage_dir, 'get_cpu_usage_comparison')
    print(f"GET CPU usage comparison saved to: {cpu_usage_dir}/get_cpu_usage_comparison.png")
    
    # ==================== 6. Memory Usage Comparison Charts ====================
    memory_usage_dir = os.path.join(output_dir, 'memory_usage')
    os.makedirs(memory_usage_dir, exist_ok=True)
    
    # PUT Memory Usage Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(x - width/2, cpu_put_mem_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_put_mem_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Memory Usage (%)', fontweight='bold')
    ax.set_title('PUT Memory Usage: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    save_figure(fig, memory_usage_dir, 'put_memory_usage_comparison')
    print(f"PUT Memory usage comparison saved to: {memory_usage_dir}/put_memory_usage_comparison.png")
    
    # GET Memory Usage Comparison
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(x - width/2, cpu_get_mem_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_get_mem_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Memory Usage (%)', fontweight='bold')
    ax.set_title('GET Memory Usage: CPU vs FPGA', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    save_figure(fig, memory_usage_dir, 'get_memory_usage_comparison')
    print(f"GET Memory usage comparison saved to: {memory_usage_dir}/get_memory_usage_comparison.png")
    
    # ==================== 7. Resource Usage Summary Chart ====================
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Resource Usage Comparison: CPU vs FPGA', fontsize=16, fontweight='bold')
    
    # Plot 1: PUT CPU Usage
    ax = axes[0, 0]
    ax.bar(x - width/2, cpu_put_cpu_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_put_cpu_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('CPU Usage (%)', fontweight='bold')
    ax.set_title('PUT CPU Usage', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 2: GET CPU Usage
    ax = axes[0, 1]
    ax.bar(x - width/2, cpu_get_cpu_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_get_cpu_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('CPU Usage (%)', fontweight='bold')
    ax.set_title('GET CPU Usage', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 3: PUT Memory Usage
    ax = axes[1, 0]
    ax.bar(x - width/2, cpu_put_mem_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_put_mem_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Memory Usage (%)', fontweight='bold')
    ax.set_title('PUT Memory Usage', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 4: GET Memory Usage
    ax = axes[1, 1]
    ax.bar(x - width/2, cpu_get_mem_usage, width, label='CPU', color='#3498db', alpha=0.8)
    ax.bar(x + width/2, fpga_get_mem_usage, width, label='FPGA', color='#e74c3c', alpha=0.8)
    ax.set_xlabel('File Size', fontweight='bold')
    ax.set_ylabel('Memory Usage (%)', fontweight='bold')
    ax.set_title('GET Memory Usage', fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, rotation=45)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'resource_usage_comparison')
    print(f"\nResource usage comparison saved to: {output_dir}/resource_usage_comparison.png")

def generate_comparison_report(cpu_results, fpga_results, output_path):
    """Generate a detailed comparison report in JSON format"""
    report = {
        'timestamp': datetime.now().isoformat(),
        'cpu_results': {
            'file': cpu_results.get('test_config', {}),
            'statistics': cpu_results.get('statistics', {})
        },
        'fpga_results': {
            'file': fpga_results.get('test_config', {}),
            'statistics': fpga_results.get('statistics', {})
        },
        'comparison': []
    }
    
    cpu_stats = cpu_results.get('statistics', {}).get('single_file', [])
    fpga_stats = fpga_results.get('statistics', {}).get('single_file', [])
    
    for cpu, fpga in zip(cpu_stats, fpga_stats):
        comparison = {
            'file_size_mb': cpu['file_size_mb'],
            'put': {
                'cpu_throughput_mbps': cpu['put_throughput_mean'],
                'fpga_throughput_mbps': fpga['put_throughput_mean'],
                'speedup': fpga['put_throughput_mean'] / cpu['put_throughput_mean'] if cpu['put_throughput_mean'] > 0 else 0,
                'cpu_latency_ms': cpu['put_latency_mean'] * 1000,
                'fpga_latency_ms': fpga['put_latency_mean'] * 1000
            },
            'get': {
                'cpu_throughput_mbps': cpu['get_throughput_mean'],
                'fpga_throughput_mbps': fpga['get_throughput_mean'],
                'speedup': fpga['get_throughput_mean'] / cpu['get_throughput_mean'] if cpu['get_throughput_mean'] > 0 else 0,
                'cpu_latency_ms': cpu['get_latency_mean'] * 1000,
                'fpga_latency_ms': fpga['get_latency_mean'] * 1000
            }
        }
        report['comparison'].append(comparison)
    
    with open(output_path, 'w') as f:
        json.dump(report, f, indent=2)
    
    print(f"\nDetailed comparison report saved to: {output_path}")
    
    # Generate plots
    output_dir = os.path.dirname(output_path)
    generate_comparison_plots(cpu_stats, fpga_stats, output_dir)

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 compare_performance.py <cpu_results.json> <fpga_results.json> [output_report.json]")
        sys.exit(1)
    
    cpu_file = sys.argv[1]
    fpga_file = sys.argv[2]
    output_file = sys.argv[3] if len(sys.argv) > 3 else 'plots/comparison_report.json'
    
    if not os.path.exists(cpu_file):
        print(f"Error: CPU results file not found: {cpu_file}")
        sys.exit(1)
    
    if not os.path.exists(fpga_file):
        print(f"Error: FPGA results file not found: {fpga_file}")
        sys.exit(1)
    
    cpu_results = load_results(cpu_file)
    fpga_results = load_results(fpga_file)
    
    compare_results(cpu_results, fpga_results)
    
    output_path = os.path.join(os.path.dirname(cpu_file), output_file) if not os.path.isabs(output_file) else output_file
    generate_comparison_report(cpu_results, fpga_results, output_path)

if __name__ == '__main__':
    main()
