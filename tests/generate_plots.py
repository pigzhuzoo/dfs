#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
性能测试结果可视化脚本
用于生成学术论文所需的图表

支持多种文件类型分析和统计显著性显示
"""

import json
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from pathlib import Path
import sys
import os


def load_results(results_file):
    """加载测试结果"""
    with open(results_file, 'r') as f:
        data = json.load(f)
        # 兼容新旧格式
        if isinstance(data, dict) and 'results' in data:
            results = data['results']
            system_info = data.get('system_info', {})
        else:
            results = data
            system_info = {}
    return results, system_info


def calculate_statistics(results):
    """计算统计信息，支持多文件类型"""
    df = pd.DataFrame(results)
    
    # 按文件大小和类型分组
    if 'file_type' in df.columns:
        grouped = df.groupby(['file_size_mb', 'file_type'])
        stats_list = []
        for (size, file_type), group in grouped:
            put_throughputs = group[group['put_success']]['put_throughput_mbps']
            get_throughputs = group[group['get_success']]['get_throughput_mbps']
            put_latencies = group[group['put_success']]['put_latency_s']
            get_latencies = group[group['get_success']]['get_latency_s']
            
            stats_list.append({
                'file_size_mb': size,
                'file_type': file_type,
                'put_throughput_mean': put_throughputs.mean() if len(put_throughputs) > 0 else 0,
                'put_throughput_std': put_throughputs.std() if len(put_throughputs) > 1 else 0,
                'put_throughput_count': len(put_throughputs),
                'get_throughput_mean': get_throughputs.mean() if len(get_throughputs) > 0 else 0,
                'get_throughput_std': get_throughputs.std() if len(get_throughputs) > 1 else 0,
                'get_throughput_count': len(get_throughputs),
                'put_latency_mean': put_latencies.mean() if len(put_latencies) > 0 else 0,
                'put_latency_std': put_latencies.std() if len(put_latencies) > 1 else 0,
                'get_latency_mean': get_latencies.mean() if len(get_latencies) > 0 else 0,
                'get_latency_std': get_latencies.std() if len(get_latencies) > 1 else 0,
                'put_success_rate': group['put_success'].mean(),
                'get_success_rate': group['get_success'].mean(),
                'sample_count': len(group)
            })
    else:
        # 旧格式兼容
        grouped = df.groupby('file_size_mb')
        stats_list = []
        for size, group in grouped:
            put_throughputs = group[group['put_success']]['put_throughput_mbps']
            get_throughputs = group[group['get_success']]['get_throughput_mbps']
            put_latencies = group[group['put_success']]['put_latency_s']
            get_latencies = group[group['get_success']]['get_latency_s']
            
            stats_list.append({
                'file_size_mb': size,
                'file_type': 'random',
                'put_throughput_mean': put_throughputs.mean() if len(put_throughputs) > 0 else 0,
                'put_throughput_std': put_throughputs.std() if len(put_throughputs) > 1 else 0,
                'put_throughput_count': len(put_throughputs),
                'get_throughput_mean': get_throughputs.mean() if len(get_throughputs) > 0 else 0,
                'get_throughput_std': get_throughputs.std() if len(get_throughputs) > 1 else 0,
                'get_throughput_count': len(get_throughputs),
                'put_latency_mean': put_latencies.mean() if len(put_latencies) > 0 else 0,
                'put_latency_std': put_latencies.std() if len(put_latencies) > 1 else 0,
                'get_latency_mean': get_latencies.mean() if len(get_latencies) > 0 else 0,
                'get_latency_std': get_latencies.std() if len(get_latencies) > 1 else 0,
                'put_success_rate': group['put_success'].mean(),
                'get_success_rate': group['get_success'].mean(),
                'sample_count': len(group)
            })
    
    return pd.DataFrame(stats_list)


def plot_throughput_comparison_by_type(stats_df, output_dir):
    """绘制按文件类型分类的吞吐量对比图"""
    plt.figure(figsize=(15, 10))
    
    file_types = stats_df['file_type'].unique()
    colors = ['red', 'blue', 'green', 'orange', 'purple']
    markers = ['o', 's', '^', 'D', 'v']
    
    for i, file_type in enumerate(file_types):
        type_data = stats_df[stats_df['file_type'] == file_type]
        file_sizes = type_data['file_size_mb'].values
        put_throughput = type_data['put_throughput_mean'].values
        get_throughput = type_data['get_throughput_mean'].values
        put_std = type_data['put_throughput_std'].values
        get_std = type_data['get_throughput_std'].values
        
        color = colors[i % len(colors)]
        marker_put = markers[i % len(markers)]
        marker_get = markers[(i + 1) % len(markers)]
        
        plt.errorbar(file_sizes, put_throughput, yerr=put_std, 
                    marker=marker_put, linestyle='-', linewidth=2, markersize=8, 
                    capsize=5, label=f'{file_type.upper()} PUT', color=color, alpha=0.8)
        plt.errorbar(file_sizes, get_throughput, yerr=get_std, 
                    marker=marker_get, linestyle='--', linewidth=2, markersize=8, 
                    capsize=5, label=f'{file_type.upper()} GET', color=color, alpha=0.8)
    
    plt.xlabel('File Size (MB)', fontsize=14)
    plt.ylabel('Throughput (MB/s)', fontsize=14)
    plt.title('DFS Throughput Performance by File Type', fontsize=16, fontweight='bold')
    plt.legend(fontsize=12, bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    plt.yscale('log')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/throughput_comparison_by_type.png', dpi=300, bbox_inches='tight')
    plt.savefig(f'{output_dir}/throughput_comparison_by_type.pdf', dpi=300, bbox_inches='tight')
    plt.close()


def plot_throughput_comparison(stats_df, output_dir):
    """绘制吞吐量对比图（合并所有类型）"""
    plt.figure(figsize=(12, 8))
    
    # 如果有多种类型，取平均值
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'put_throughput_mean': 'mean',
            'put_throughput_std': 'mean',
            'get_throughput_mean': 'mean',
            'get_throughput_std': 'mean',
            'put_success_rate': 'mean',
            'get_success_rate': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    file_sizes = merged_df['file_size_mb'].values
    put_throughput = merged_df['put_throughput_mean'].values
    get_throughput = merged_df['get_throughput_mean'].values
    put_std = merged_df['put_throughput_std'].values
    get_std = merged_df['get_throughput_std'].values
    
    plt.errorbar(file_sizes, put_throughput, yerr=put_std, 
                marker='o', linestyle='-', linewidth=2, markersize=8, 
                capsize=5, label='PUT Throughput', alpha=0.8)
    plt.errorbar(file_sizes, get_throughput, yerr=get_std, 
                marker='s', linestyle='--', linewidth=2, markersize=8, 
                capsize=5, label='GET Throughput', alpha=0.8)
    
    plt.xlabel('File Size (MB)', fontsize=12)
    plt.ylabel('Throughput (MB/s)', fontsize=12)
    plt.title('DFS Throughput Performance vs File Size', fontsize=14, fontweight='bold')
    plt.legend(fontsize=12)
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    plt.yscale('log')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/throughput_comparison.png', dpi=300, bbox_inches='tight')
    plt.savefig(f'{output_dir}/throughput_comparison.pdf', dpi=300, bbox_inches='tight')
    plt.close()


def plot_latency_comparison(stats_df, output_dir):
    """绘制时延对比图"""
    plt.figure(figsize=(12, 8))
    
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'put_latency_mean': 'mean',
            'put_latency_std': 'mean',
            'get_latency_mean': 'mean',
            'get_latency_std': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    file_sizes = merged_df['file_size_mb'].values
    put_latency = merged_df['put_latency_mean'].values
    get_latency = merged_df['get_latency_mean'].values
    put_std = merged_df['put_latency_std'].values
    get_std = merged_df['get_latency_std'].values
    
    plt.errorbar(file_sizes, put_latency, yerr=put_std, 
                marker='o', linestyle='-', linewidth=2, markersize=8, 
                capsize=5, label='PUT Latency', alpha=0.8)
    plt.errorbar(file_sizes, get_latency, yerr=get_std, 
                marker='s', linestyle='--', linewidth=2, markersize=8, 
                capsize=5, label='GET Latency', alpha=0.8)
    
    plt.xlabel('File Size (MB)', fontsize=12)
    plt.ylabel('Latency (seconds)', fontsize=12)
    plt.title('DFS Latency Performance vs File Size', fontsize=14, fontweight='bold')
    plt.legend(fontsize=12)
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    plt.yscale('log')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/latency_comparison.png', dpi=300, bbox_inches='tight')
    plt.savefig(f'{output_dir}/latency_comparison.pdf', dpi=300, bbox_inches='tight')
    plt.close()


def plot_success_rate(stats_df, output_dir):
    """绘制成功率图"""
    plt.figure(figsize=(12, 6))
    
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'put_success_rate': 'mean',
            'get_success_rate': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    file_sizes = merged_df['file_size_mb'].values
    put_success = merged_df['put_success_rate'].values * 100
    get_success = merged_df['get_success_rate'].values * 100
    
    plt.plot(file_sizes, put_success, marker='o', linestyle='-', 
             linewidth=2, markersize=8, label='PUT Success Rate')
    plt.plot(file_sizes, get_success, marker='s', linestyle='--', 
             linewidth=2, markersize=8, label='GET Success Rate')
    
    plt.xlabel('File Size (MB)', fontsize=12)
    plt.ylabel('Success Rate (%)', fontsize=12)
    plt.title('DFS Operation Success Rate vs File Size', fontsize=14, fontweight='bold')
    plt.legend(fontsize=12)
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/success_rate.png', dpi=300, bbox_inches='tight')
    plt.savefig(f'{output_dir}/success_rate.pdf', dpi=300, bbox_inches='tight')
    plt.close()


def plot_resource_usage(stats_df, output_dir):
    """绘制资源使用情况图"""
    if 'put_cpu_usage' not in stats_df.columns:
        return
    
    plt.figure(figsize=(15, 10))
    
    # CPU使用率
    plt.subplot(2, 2, 1)
    file_sizes = stats_df['file_size_mb'].values
    put_cpu = stats_df['put_cpu_usage'].values
    get_cpu = stats_df['get_cpu_usage'].values
    
    plt.plot(file_sizes, put_cpu, marker='o', linestyle='-', label='PUT CPU')
    plt.plot(file_sizes, get_cpu, marker='s', linestyle='--', label='GET CPU')
    plt.xlabel('File Size (MB)')
    plt.ylabel('CPU Usage (%)')
    plt.title('CPU Usage by File Size')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    
    # 内存使用率
    plt.subplot(2, 2, 2)
    put_mem = stats_df['put_memory_usage'].values
    get_mem = stats_df['get_memory_usage'].values
    
    plt.plot(file_sizes, put_mem, marker='o', linestyle='-', label='PUT Memory')
    plt.plot(file_sizes, get_mem, marker='s', linestyle='--', label='GET Memory')
    plt.xlabel('File Size (MB)')
    plt.ylabel('Memory Usage (%)')
    plt.title('Memory Usage by File Size')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.xscale('log')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/resource_usage.png', dpi=300, bbox_inches='tight')
    plt.savefig(f'{output_dir}/resource_usage.pdf', dpi=300, bbox_inches='tight')
    plt.close()


def generate_summary_table(stats_df, output_file, system_info=None):
    """生成汇总表格（学术论文格式）"""
    summary_data = []
    
    if 'file_type' in stats_df.columns:
        # 按文件类型和大小分组
        for _, row in stats_df.iterrows():
            summary_data.append([
                f"{row['file_size_mb']} MB",
                f"{row['file_type']}",
                f"{row['put_throughput_mean']:.2f} ± {row['put_throughput_std']:.2f}",
                f"{row['get_throughput_mean']:.2f} ± {row['get_throughput_std']:.2f}",
                f"{row['put_latency_mean']:.3f} ± {row['put_latency_std']:.3f}",
                f"{row['get_latency_mean']:.3f} ± {row['get_latency_std']:.3f}",
                f"{row['put_success_rate']*100:.1f}%",
                f"{row['get_success_rate']*100:.1f}%"
            ])
        
        headers = ['File Size', 'File Type', 'PUT Throughput (MB/s)', 'GET Throughput (MB/s)', 
                   'PUT Latency (s)', 'GET Latency (s)', 'PUT Success Rate', 'GET Success Rate']
    else:
        # 旧格式
        for _, row in stats_df.iterrows():
            summary_data.append([
                f"{row['file_size_mb']} MB",
                f"{row['put_throughput_mean']:.2f} ± {row['put_throughput_std']:.2f}",
                f"{row['get_throughput_mean']:.2f} ± {row['get_throughput_std']:.2f}",
                f"{row['put_latency_mean']:.3f} ± {row['put_latency_std']:.3f}",
                f"{row['get_latency_mean']:.3f} ± {row['get_latency_std']:.3f}",
                f"{row['put_success_rate']*100:.1f}%",
                f"{row['get_success_rate']*100:.1f}%"
            ])
        
        headers = ['File Size', 'PUT Throughput (MB/s)', 'GET Throughput (MB/s)', 
                   'PUT Latency (s)', 'GET Latency (s)', 'PUT Success Rate', 'GET Success Rate']
    
    # 创建LaTeX表格
    latex_lines = []
    latex_lines.append("\\begin{table}[htbp]")
    latex_lines.append("\\centering")
    latex_lines.append("\\caption{DFS Performance Metrics}")
    latex_lines.append("\\label{tab:performance}")
    
    if 'file_type' in stats_df.columns:
        latex_lines.append("\\begin{tabular}{lccccccc}")
    else:
        latex_lines.append("\\begin{tabular}{lcccccc}")
    
    latex_lines.append("\\toprule")
    latex_lines.append(" & ".join(headers) + " \\\\")
    latex_lines.append("\\midrule")
    for row in summary_data:
        latex_lines.append(" & ".join(row) + " \\\\")
    latex_lines.append("\\bottomrule")
    latex_lines.append("\\end{tabular}")
    latex_lines.append("\\end{table}")
    
    with open(output_file, 'w') as f:
        f.write('\n'.join(latex_lines))
    
    # 创建系统信息文件
    if system_info:
        with open(output_file.replace('.tex', '_system_info.txt'), 'w') as f:
            f.write("System Information for Reproducibility:\n")
            f.write("=" * 50 + "\n")
            for key, value in system_info.items():
                if key != 'environment':
                    f.write(f"{key}: {value}\n")


def generate_plots(results_file):
    """主函数：生成所有图表"""
    if not Path(results_file).exists():
        print(f"Results file {results_file} not found!")
        return
    
    output_dir = "plots"
    Path(output_dir).mkdir(exist_ok=True)
    
    print(f"Loading results from {results_file}...")
    results, system_info = load_results(results_file)
    stats_df = calculate_statistics(results)
    
    print("Generating plots...")
    plot_throughput_comparison(stats_df, output_dir)
    plot_latency_comparison(stats_df, output_dir)
    plot_success_rate(stats_df, output_dir)
    plot_resource_usage(stats_df, output_dir)
    
    # 如果有多种文件类型，生成详细对比图
    if 'file_type' in stats_df.columns and len(stats_df['file_type'].unique()) > 1:
        plot_throughput_comparison_by_type(stats_df, output_dir)
    
    generate_summary_table(stats_df, f"{output_dir}/performance_table.tex", system_info)
    
    # 打印一些统计信息
    print("\nPerformance Summary:")
    print("=" * 100)
    if 'file_type' in stats_df.columns:
        for _, row in stats_df.iterrows():
            print(f"File Size: {row['file_size_mb']:>6} MB | "
                  f"Type: {row['file_type']:<8} | "
                  f"PUT: {row['put_throughput_mean']:>6.2f} MB/s | "
                  f"GET: {row['get_throughput_mean']:>6.2f} MB/s | "
                  f"PUT Latency: {row['put_latency_mean']:>6.3f}s | "
                  f"GET Latency: {row['get_latency_mean']:>6.3f}s")
    else:
        for _, row in stats_df.iterrows():
            print(f"File Size: {row['file_size_mb']:>6} MB | "
                  f"PUT: {row['put_throughput_mean']:>6.2f} MB/s | "
                  f"GET: {row['get_throughput_mean']:>6.2f} MB/s | "
                  f"PUT Latency: {row['put_latency_mean']:>6.3f}s | "
                  f"GET Latency: {row['get_latency_mean']:>6.3f}s")
    
    print(f"\nAll plots saved to {output_dir}/ directory")
    print(f"LaTeX table saved to {output_dir}/performance_table.tex")
    
    if system_info:
        print(f"System info saved to {output_dir}/performance_table_system_info.txt")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        results_file = sys.argv[1]
    else:
        results_file = "performance_results.json"
    
    generate_plots(results_file)