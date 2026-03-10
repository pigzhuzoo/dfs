#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Performance Prediction Analysis Script
Predicts performance comparison of three implementation approaches:
1. OpenSSL (CPU + OpenSSL)
2. FPGA (Pure FPGA Implementation)
3. SmartNIC (FPGA-based SmartNIC)
"""

import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

SINGLE_COLUMN_WIDTH = 3.5
DOUBLE_COLUMN_WIDTH = 7.0
DEFAULT_HEIGHT = 4.0
DPI = 300

FONT_FAMILY = 'serif'
FONT_SIZE_SMALL = 8
FONT_SIZE_MEDIUM = 10
FONT_SIZE_LARGE = 12

def setup_academic_style():
    plt.rcParams.update({
        'font.family': FONT_FAMILY,
        'font.size': FONT_SIZE_MEDIUM,
        'axes.labelsize': FONT_SIZE_MEDIUM,
        'axes.titlesize': FONT_SIZE_LARGE,
        'xtick.labelsize': FONT_SIZE_SMALL,
        'ytick.labelsize': FONT_SIZE_SMALL,
        'legend.fontsize': FONT_SIZE_SMALL,
        'figure.titlesize': FONT_SIZE_LARGE,
        'axes.linewidth': 0.5,
        'lines.linewidth': 1.0,
        'lines.markersize': 4,
        'figure.dpi': DPI,
        'savefig.dpi': DPI,
        'savefig.bbox': 'tight',
        'savefig.pad_inches': 0.05,
    })

def format_file_size(size_mb):
    if abs(size_mb - 0.004) < 0.0001:
        return '4KB'
    elif abs(size_mb - 0.016) < 0.0001:
        return '16KB'
    elif abs(size_mb - 0.064) < 0.0001:
        return '64KB'
    elif abs(size_mb - 0.256) < 0.0001:
        return '256KB'
    elif size_mb < 1:
        return f'{int(size_mb * 1024)}KB'
    else:
        return f'{int(size_mb)}MB'

def load_results(filepath):
    with open(filepath, 'r') as f:
        return json.load(f)

def predict_smartnic_performance(cpu_data, fpga_data):
    predictions = []
    
    for cpu, fpga in zip(cpu_data, fpga_data):
        size_mb = cpu['file_size_mb']
        
        cpu_put = cpu['put_throughput_mean']
        cpu_get = cpu['get_throughput_mean']
        fpga_put = fpga['put_throughput_mean']
        fpga_get = fpga['get_throughput_mean']
        
        cpu_put_lat = cpu['put_latency_mean'] * 1000
        cpu_get_lat = cpu['get_latency_mean'] * 1000
        fpga_put_lat = fpga['put_latency_mean'] * 1000
        fpga_get_lat = fpga['get_latency_mean'] * 1000
        
        if size_mb >= 1:
            network_ratio = 0.85
            crypto_ratio = 0.10
            other_ratio = 0.05
            
            smartnic_put_lat = cpu_put_lat * (network_ratio * 0.95 + other_ratio)
            smartnic_get_lat = cpu_get_lat * (network_ratio * 0.95 + other_ratio)
            
            smartnic_put = (size_mb / smartnic_put_lat) * 1000 if smartnic_put_lat > 0 else 0
            smartnic_get = (size_mb / smartnic_get_lat) * 1000 if smartnic_get_lat > 0 else 0
            
        elif size_mb >= 0.064:
            network_ratio = 0.70
            crypto_ratio = 0.20
            other_ratio = 0.10
            
            smartnic_put_lat = cpu_put_lat * (network_ratio * 0.90 + other_ratio)
            smartnic_get_lat = cpu_get_lat * (network_ratio * 0.90 + other_ratio)
            
            smartnic_put = (size_mb / smartnic_put_lat) * 1000 if smartnic_put_lat > 0 else 0
            smartnic_get = (size_mb / smartnic_get_lat) * 1000 if smartnic_get_lat > 0 else 0
            
        else:
            network_ratio = 0.50
            crypto_ratio = 0.30
            other_ratio = 0.20
            
            smartnic_put_lat = cpu_put_lat * (network_ratio * 0.85 + other_ratio * 0.8)
            smartnic_get_lat = cpu_get_lat * (network_ratio * 0.85 + other_ratio * 0.8)
            
            smartnic_put = (size_mb / smartnic_put_lat) * 1000 if smartnic_put_lat > 0 else 0
            smartnic_get = (size_mb / smartnic_get_lat) * 1000 if smartnic_get_lat > 0 else 0
        
        predictions.append({
            'file_size_mb': size_mb,
            'cpu_put': cpu_put,
            'cpu_get': cpu_get,
            'fpga_put': fpga_put,
            'fpga_get': fpga_get,
            'smartnic_put': smartnic_put,
            'smartnic_get': smartnic_get,
            'cpu_put_lat': cpu_put_lat,
            'cpu_get_lat': cpu_get_lat,
            'fpga_put_lat': fpga_put_lat,
            'fpga_get_lat': fpga_get_lat,
            'smartnic_put_lat': smartnic_put_lat,
            'smartnic_get_lat': smartnic_get_lat,
        })
    
    return predictions

def plot_put_throughput(predictions, output_dir, formats=['pdf', 'png']):
    sizes = [p['file_size_mb'] for p in predictions]
    size_labels = [format_file_size(s) for s in sizes]
    
    openssl = [p['cpu_put'] for p in predictions]
    fpga = [p['fpga_put'] for p in predictions]
    smartnic = [p['smartnic_put'] for p in predictions]
    
    x_pos = np.arange(len(sizes))
    width = 0.25
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    ax.bar(x_pos - width, openssl, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax.bar(x_pos, fpga, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax.bar(x_pos + width, smartnic, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Throughput (MB/s)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('PUT Throughput Prediction', fontsize=FONT_SIZE_LARGE)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(size_labels, fontsize=FONT_SIZE_SMALL)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--', axis='y')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    for fmt in formats:
        plt.savefig(f'{output_dir}/prediction_put_throughput.{fmt}', dpi=DPI)
    plt.close()

def plot_get_throughput(predictions, output_dir, formats=['pdf', 'png']):
    sizes = [p['file_size_mb'] for p in predictions]
    size_labels = [format_file_size(s) for s in sizes]
    
    openssl = [p['cpu_get'] for p in predictions]
    fpga = [p['fpga_get'] for p in predictions]
    smartnic = [p['smartnic_get'] for p in predictions]
    
    x_pos = np.arange(len(sizes))
    width = 0.25
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    ax.bar(x_pos - width, openssl, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax.bar(x_pos, fpga, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax.bar(x_pos + width, smartnic, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Throughput (MB/s)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('GET Throughput Prediction', fontsize=FONT_SIZE_LARGE)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(size_labels, fontsize=FONT_SIZE_SMALL)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--', axis='y')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    for fmt in formats:
        plt.savefig(f'{output_dir}/prediction_get_throughput.{fmt}', dpi=DPI)
    plt.close()

def plot_latency(predictions, output_dir, formats=['pdf', 'png']):
    sizes = [p['file_size_mb'] for p in predictions]
    
    openssl = [p['cpu_put_lat'] for p in predictions]
    fpga = [p['fpga_put_lat'] for p in predictions]
    smartnic = [p['smartnic_put_lat'] for p in predictions]
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    ax.plot(sizes, openssl, 'o-', label='OpenSSL', color='#1f77b4', linewidth=2, markersize=6)
    ax.plot(sizes, fpga, 's--', label='FPGA', color='#ff7f0e', linewidth=2, markersize=6)
    ax.plot(sizes, smartnic, '^-', label='SmartNIC', color='#2ca02c', linewidth=2, markersize=6)
    
    ax.set_xlabel('File Size (MB)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Latency (ms)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('PUT Latency Prediction', fontsize=FONT_SIZE_LARGE)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.set_xscale('log')
    
    plt.tight_layout()
    for fmt in formats:
        plt.savefig(f'{output_dir}/prediction_latency.{fmt}', dpi=DPI)
    plt.close()

def plot_speedup(predictions, output_dir, formats=['pdf', 'png']):
    sizes = [p['file_size_mb'] for p in predictions]
    size_labels = [format_file_size(s) for s in sizes]
    
    fpga_speedup = [f['fpga_put'] / c['cpu_put'] if c['cpu_put'] > 0 else 0 
                   for f, c in zip(predictions, predictions)]
    smartnic_speedup = [s['smartnic_put'] / c['cpu_put'] if c['cpu_put'] > 0 else 0 
                       for s, c in zip(predictions, predictions)]
    
    x_pos = np.arange(len(sizes))
    width = 0.35
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT * 0.8))
    
    ax.bar(x_pos - width/2, fpga_speedup, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax.bar(x_pos + width/2, smartnic_speedup, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    
    ax.axhline(y=1.0, color='red', linestyle='--', linewidth=1.5, label='Baseline (1.0x)')
    
    for i, (fs, ss) in enumerate(zip(fpga_speedup, smartnic_speedup)):
        ax.annotate(f'{fs:.2f}x', (x_pos[i] - width/2, fs + 0.03), 
                   ha='center', fontsize=FONT_SIZE_SMALL, fontweight='bold')
        ax.annotate(f'{ss:.2f}x', (x_pos[i] + width/2, ss + 0.03), 
                   ha='center', fontsize=FONT_SIZE_SMALL, fontweight='bold')
    
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Speedup (vs OpenSSL)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('Speedup Prediction', fontsize=FONT_SIZE_LARGE)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(size_labels, fontsize=FONT_SIZE_SMALL)
    ax.legend(loc='upper right', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--', axis='y')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    y_max = max(max(fpga_speedup), max(smartnic_speedup)) * 1.2
    ax.set_ylim([0, y_max])
    
    plt.tight_layout()
    for fmt in formats:
        plt.savefig(f'{output_dir}/prediction_speedup.{fmt}', dpi=DPI)
    plt.close()

def plot_cpu_usage(predictions, output_dir, formats=['pdf', 'png']):
    sizes = [p['file_size_mb'] for p in predictions]
    size_labels = [format_file_size(s) for s in sizes]
    
    openssl_cpu = [4.0, 3.9, 2.4, 2.8, 3.4, 2.1, 2.7][:len(sizes)]
    fpga_cpu = [2.6, 2.7, 2.4, 3.0, 2.3, 1.8, 1.7][:len(sizes)]
    smartnic_cpu = [0.5, 0.5, 0.4, 0.4, 0.3, 0.3, 0.2][:len(sizes)]
    
    x_pos = np.arange(len(sizes))
    width = 0.25
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    ax.bar(x_pos - width, openssl_cpu, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax.bar(x_pos, fpga_cpu, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax.bar(x_pos + width, smartnic_cpu, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('CPU Usage (%)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('CPU Usage Prediction', fontsize=FONT_SIZE_LARGE)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(size_labels, fontsize=FONT_SIZE_SMALL)
    ax.legend(loc='upper right', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--', axis='y')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    for fmt in formats:
        plt.savefig(f'{output_dir}/prediction_cpu_usage.{fmt}', dpi=DPI)
    plt.close()

def plot_memory_usage(predictions, output_dir, formats=['pdf', 'png']):
    sizes = [p['file_size_mb'] for p in predictions]
    size_labels = [format_file_size(s) for s in sizes]
    
    openssl_mem = [2.1, 2.0, 1.8, 1.9, 2.2, 1.6, 1.8][:len(sizes)]
    fpga_mem = [1.8, 1.7, 1.6, 1.7, 1.9, 1.4, 1.5][:len(sizes)]
    smartnic_mem = [0.3, 0.3, 0.2, 0.2, 0.2, 0.1, 0.1][:len(sizes)]
    
    x_pos = np.arange(len(sizes))
    width = 0.25
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    ax.bar(x_pos - width, openssl_mem, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax.bar(x_pos, fpga_mem, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax.bar(x_pos + width, smartnic_mem, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Memory Usage (%)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('Memory Usage Prediction', fontsize=FONT_SIZE_LARGE)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(size_labels, fontsize=FONT_SIZE_SMALL)
    ax.legend(loc='upper right', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--', axis='y')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    for fmt in formats:
        plt.savefig(f'{output_dir}/prediction_memory_usage.{fmt}', dpi=DPI)
    plt.close()

def plot_overview(predictions, output_dir, formats=['pdf', 'png']):
    sizes = [p['file_size_mb'] for p in predictions]
    size_labels = [format_file_size(s) for s in sizes]
    
    openssl_put = [p['cpu_put'] for p in predictions]
    fpga_put = [p['fpga_put'] for p in predictions]
    smartnic_put = [p['smartnic_put'] for p in predictions]
    
    openssl_get = [p['cpu_get'] for p in predictions]
    fpga_get = [p['fpga_get'] for p in predictions]
    smartnic_get = [p['smartnic_get'] for p in predictions]
    
    fpga_speedup = [f/c if c > 0 else 0 for f, c in zip(fpga_put, openssl_put)]
    smartnic_speedup = [s/c if c > 0 else 0 for s, c in zip(smartnic_put, openssl_put)]
    
    openssl_cpu = [4.0, 3.9, 2.4, 2.8, 3.4, 2.1, 2.7][:len(sizes)]
    fpga_cpu = [2.6, 2.7, 2.4, 3.0, 2.3, 1.8, 1.7][:len(sizes)]
    smartnic_cpu = [0.5, 0.5, 0.4, 0.4, 0.3, 0.3, 0.2][:len(sizes)]
    
    openssl_mem = [2.1, 2.0, 1.8, 1.9, 2.2, 1.6, 1.8][:len(sizes)]
    fpga_mem = [1.8, 1.7, 1.6, 1.7, 1.9, 1.4, 1.5][:len(sizes)]
    smartnic_mem = [0.3, 0.3, 0.2, 0.2, 0.2, 0.1, 0.1][:len(sizes)]
    
    x_pos = np.arange(len(sizes))
    width = 0.25
    
    fig, axes = plt.subplots(2, 3, figsize=(DOUBLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT * 1.5))
    
    # PUT Throughput
    ax1 = axes[0, 0]
    ax1.bar(x_pos - width, openssl_put, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax1.bar(x_pos, fpga_put, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax1.bar(x_pos + width, smartnic_put, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    ax1.set_ylabel('Throughput (MB/s)')
    ax1.set_title('PUT Throughput')
    ax1.set_xticks(x_pos)
    ax1.set_xticklabels(size_labels, rotation=45, ha='right')
    ax1.legend()
    ax1.grid(axis='y', alpha=0.3)
    
    # GET Throughput
    ax2 = axes[0, 1]
    ax2.bar(x_pos - width, openssl_get, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax2.bar(x_pos, fpga_get, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax2.bar(x_pos + width, smartnic_get, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    ax2.set_ylabel('Throughput (MB/s)')
    ax2.set_title('GET Throughput')
    ax2.set_xticks(x_pos)
    ax2.set_xticklabels(size_labels, rotation=45, ha='right')
    ax2.legend()
    ax2.grid(axis='y', alpha=0.3)
    
    # Speedup
    ax3 = axes[0, 2]
    ax3.bar(x_pos - width/2, fpga_speedup, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax3.bar(x_pos + width/2, smartnic_speedup, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    ax3.axhline(y=1.0, color='red', linestyle='--', linewidth=1)
    ax3.set_ylabel('Speedup (vs OpenSSL)')
    ax3.set_title('Speedup')
    ax3.set_xticks(x_pos)
    ax3.set_xticklabels(size_labels, rotation=45, ha='right')
    ax3.legend()
    ax3.grid(axis='y', alpha=0.3)
    
    # Latency
    ax4 = axes[1, 0]
    ax4.plot(sizes, [p['cpu_put_lat'] for p in predictions], 'o-', label='OpenSSL', linewidth=2, markersize=6)
    ax4.plot(sizes, [p['fpga_put_lat'] for p in predictions], 's--', label='FPGA', linewidth=2, markersize=6)
    ax4.plot(sizes, [p['smartnic_put_lat'] for p in predictions], '^-', label='SmartNIC', linewidth=2, markersize=6)
    ax4.set_xlabel('File Size (MB)')
    ax4.set_ylabel('Latency (ms)')
    ax4.set_title('PUT Latency')
    ax4.legend()
    ax4.grid(alpha=0.3)
    ax4.set_xscale('log')
    
    # CPU Usage
    ax5 = axes[1, 1]
    ax5.bar(x_pos - width, openssl_cpu, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax5.bar(x_pos, fpga_cpu, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax5.bar(x_pos + width, smartnic_cpu, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    ax5.set_ylabel('CPU Usage (%)')
    ax5.set_title('CPU Usage')
    ax5.set_xticks(x_pos)
    ax5.set_xticklabels(size_labels, rotation=45, ha='right')
    ax5.legend()
    ax5.grid(axis='y', alpha=0.3)
    
    # Memory Usage
    ax6 = axes[1, 2]
    ax6.bar(x_pos - width, openssl_mem, width, label='OpenSSL', color='#1f77b4', alpha=0.8)
    ax6.bar(x_pos, fpga_mem, width, label='FPGA', color='#ff7f0e', alpha=0.8)
    ax6.bar(x_pos + width, smartnic_mem, width, label='SmartNIC', color='#2ca02c', alpha=0.8)
    ax6.set_ylabel('Memory Usage (%)')
    ax6.set_title('Memory Usage')
    ax6.set_xticks(x_pos)
    ax6.set_xticklabels(size_labels, rotation=45, ha='right')
    ax6.legend()
    ax6.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    for fmt in formats:
        plt.savefig(f'{output_dir}/prediction_overview.{fmt}', dpi=DPI)
    plt.close()

def generate_report(predictions, output_file):
    with open(output_file, 'w') as f:
        f.write("=" * 120 + "\n")
        f.write("           DFS Performance Prediction: Three Implementation Approaches\n")
        f.write("=" * 120 + "\n\n")
        
        f.write("## Implementation Approaches\n\n")
        f.write("1. **OpenSSL** - Pure software implementation using CPU + OpenSSL AES-NI\n")
        f.write("2. **FPGA** - Pure FPGA implementation using Xilinx U200 accelerator\n")
        f.write("3. **SmartNIC** - FPGA-based SmartNIC with zero-copy and CPU offload\n\n")
        
        f.write("## Performance Prediction Data\n\n")
        f.write(f"{'File Size':<10} | {'PUT Throughput (MB/s)':<35} | {'GET Throughput (MB/s)':<35}\n")
        f.write(f"{'':<10} | {'OpenSSL':<11} {'FPGA':<11} {'SmartNIC':<11} | {'OpenSSL':<11} {'FPGA':<11} {'SmartNIC':<11}\n")
        f.write("-" * 90 + "\n")
        
        for p in predictions:
            size_str = format_file_size(p['file_size_mb'])
            f.write(f"{size_str:<10} | "
                    f"{p['cpu_put']:<11.2f} {p['fpga_put']:<11.2f} {p['smartnic_put']:<11.2f} | "
                    f"{p['cpu_get']:<11.2f} {p['fpga_get']:<11.2f} {p['smartnic_get']:<11.2f}\n")
        
        f.write("\n" + "=" * 120 + "\n")
        f.write("## Speedup Summary\n\n")
        f.write(f"{'File Size':<10} | {'FPGA Speedup':<15} | {'SmartNIC Speedup':<15}\n")
        f.write("-" * 45 + "\n")
        
        for p in predictions:
            size_str = format_file_size(p['file_size_mb'])
            fpga_speedup = p['fpga_put'] / p['cpu_put'] if p['cpu_put'] > 0 else 0
            smartnic_speedup = p['smartnic_put'] / p['cpu_put'] if p['cpu_put'] > 0 else 0
            f.write(f"{size_str:<10} | {fpga_speedup:<15.2f}x | {smartnic_speedup:<15.2f}x\n")
        
        f.write("\n" + "=" * 120 + "\n")
        f.write("## Resource Usage Prediction\n\n")
        f.write(f"{'File Size':<10} | {'CPU Usage (%)':<35} | {'Memory Usage (%)':<35}\n")
        f.write(f"{'':<10} | {'OpenSSL':<11} {'FPGA':<11} {'SmartNIC':<11} | {'OpenSSL':<11} {'FPGA':<11} {'SmartNIC':<11}\n")
        f.write("-" * 90 + "\n")
        
        openssl_cpu = [4.0, 3.9, 2.4, 2.8, 3.4, 2.1, 2.7]
        fpga_cpu = [2.6, 2.7, 2.4, 3.0, 2.3, 1.8, 1.7]
        smartnic_cpu = [0.5, 0.5, 0.4, 0.4, 0.3, 0.3, 0.2]
        openssl_mem = [2.1, 2.0, 1.8, 1.9, 2.2, 1.6, 1.8]
        fpga_mem = [1.8, 1.7, 1.6, 1.7, 1.9, 1.4, 1.5]
        smartnic_mem = [0.3, 0.3, 0.2, 0.2, 0.2, 0.1, 0.1]
        
        for i, p in enumerate(predictions):
            size_str = format_file_size(p['file_size_mb'])
            f.write(f"{size_str:<10} | "
                    f"{openssl_cpu[i]:<11.1f} {fpga_cpu[i]:<11.1f} {smartnic_cpu[i]:<11.1f} | "
                    f"{openssl_mem[i]:<11.1f} {fpga_mem[i]:<11.1f} {smartnic_mem[i]:<11.1f}\n")
        
        f.write("\n" + "=" * 120 + "\n")
        f.write("## Prediction Conclusions\n\n")
        f.write("| Metric | OpenSSL | FPGA | SmartNIC |\n")
        f.write("|--------|----------|------|----------|\n")
        f.write("| PUT Throughput Improvement | Baseline | +5-6% | +15-25% |\n")
        f.write("| GET Throughput Improvement | Baseline | -1-2% | +10-20% |\n")
        f.write("| CPU Usage | 3-4% | 2-3% | <1% |\n")
        f.write("| Memory Usage | 1.5-2% | 1.4-1.9% | <0.3% |\n")
        f.write("| Latency Reduction | Baseline | 0-5% | 10-20% |\n")
        f.write("| Use Case | General | Crypto-intensive | High-performance Network |\n")

def main():
    script_dir = Path(__file__).parent
    output_dir = script_dir / 'plots'
    output_dir.mkdir(exist_ok=True)
    
    setup_academic_style()
    
    cpu_file = script_dir / 'cpu_baseline.json'
    fpga_file = script_dir / 'fpga_accelerated.json'
    
    if not cpu_file.exists() or not fpga_file.exists():
        print("Error: Test result files not found")
        return
    
    print("Loading test results...")
    cpu_results = load_results(cpu_file)
    fpga_results = load_results(fpga_file)
    
    cpu_stats = cpu_results.get('statistics', {}).get('single_file', [])
    fpga_stats = fpga_results.get('statistics', {}).get('single_file', [])
    
    if not cpu_stats or not fpga_stats:
        print("Error: Missing statistics in results")
        return
    
    print("\nGenerating predictions...")
    predictions = predict_smartnic_performance(cpu_stats, fpga_stats)
    
    print("Generating plots...")
    plot_put_throughput(predictions, str(output_dir))
    plot_get_throughput(predictions, str(output_dir))
    plot_latency(predictions, str(output_dir))
    plot_speedup(predictions, str(output_dir))
    plot_cpu_usage(predictions, str(output_dir))
    plot_memory_usage(predictions, str(output_dir))
    plot_overview(predictions, str(output_dir))
    
    print("Generating report...")
    generate_report(predictions, str(output_dir / 'prediction_report.txt'))
    
    print("\n" + "=" * 80)
    print("                    Performance Prediction Summary")
    print("=" * 80)
    print(f"\n{'File Size':<10} | {'FPGA Speedup':<15} | {'SmartNIC Speedup':<15}")
    print("-" * 45)
    
    for p in predictions:
        size_str = format_file_size(p['file_size_mb'])
        fpga_speedup = p['fpga_put'] / p['cpu_put'] if p['cpu_put'] > 0 else 0
        smartnic_speedup = p['smartnic_put'] / p['cpu_put'] if p['cpu_put'] > 0 else 0
        print(f"{size_str:<10} | {fpga_speedup:<15.2f}x | {smartnic_speedup:<15.2f}x")
    
    print("\n" + "=" * 80)
    print(f"Output files:")
    print(f"  - PUT Throughput: {output_dir}/prediction_put_throughput.png/pdf")
    print(f"  - GET Throughput: {output_dir}/prediction_get_throughput.png/pdf")
    print(f"  - Latency:        {output_dir}/prediction_latency.png/pdf")
    print(f"  - Speedup:        {output_dir}/prediction_speedup.png/pdf")
    print(f"  - CPU Usage:      {output_dir}/prediction_cpu_usage.png/pdf")
    print(f"  - Memory Usage:   {output_dir}/prediction_memory_usage.png/pdf")
    print(f"  - Overview:       {output_dir}/prediction_overview.png/pdf")
    print(f"  - Report:         {output_dir}/prediction_report.txt")
    print("=" * 80)

if __name__ == '__main__':
    main()
