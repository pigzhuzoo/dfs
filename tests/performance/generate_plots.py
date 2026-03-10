#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
性能测试结果可视化脚本
用于生成学术论文所需的图表

学术规范特性：
1. 使用学术期刊标准字体（Times New Roman / serif）
2. 标准图表尺寸（单栏：3.5英寸，双栏：7英寸）
3. 置信区间阴影区域显示
4. 改进的误差棒显示
5. 多种输出格式支持（PDF、PNG、EPS、SVG）
6. IEEE/ACM 格式 LaTeX 表格
7. 统计检验结果展示
"""

import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import numpy as np
import pandas as pd
from pathlib import Path
import sys
import os
import math

try:
    from scipy import stats
    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False

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
        'errorbar.capsize': 2,
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

def format_file_sizes(sizes_mb):
    return [format_file_size(s) for s in sizes_mb]

def perform_statistical_test(group1, group2, test_type='ttest'):
    if not group1 or not group2 or len(group1) < 2 or len(group2) < 2:
        return None, None
    if HAS_SCIPY:
        if test_type == 'ttest':
            stat, pvalue = stats.ttest_ind(group1, group2)
            return stat, pvalue
        elif test_type == 'mannwhitney':
            stat, pvalue = stats.mannwhitneyu(group1, group2, alternative='two-sided')
            return stat, pvalue
    return None, None

def load_results(results_file):
    with open(results_file, 'r') as f:
        data = json.load(f)
        if isinstance(data, dict) and 'results' in data:
            results = data['results']
            system_info = data.get('system_info', {})
            multi_file_results = data.get('multi_file_tests', [])
            statistics = data.get('statistics', {})
        else:
            results = data
            system_info = {}
            multi_file_results = []
            statistics = {}
    return results, multi_file_results, system_info, statistics

def calculate_statistics(results):
    df = pd.DataFrame(results)
    
    if df.empty:
        return pd.DataFrame()
    
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
                'put_cpu_usage': group['put_cpu_usage'].mean() if 'put_cpu_usage' in group.columns else 0,
                'get_cpu_usage': group['get_cpu_usage'].mean() if 'get_cpu_usage' in group.columns else 0,
                'put_memory_usage': group['put_memory_usage'].mean() if 'put_memory_usage' in group.columns else 0,
                'get_memory_usage': group['get_memory_usage'].mean() if 'get_memory_usage' in group.columns else 0,
                'sample_count': len(group)
            })
    else:
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
                'put_cpu_usage': group['put_cpu_usage'].mean() if 'put_cpu_usage' in group.columns else 0,
                'get_cpu_usage': group['get_cpu_usage'].mean() if 'get_cpu_usage' in group.columns else 0,
                'put_memory_usage': group['put_memory_usage'].mean() if 'put_memory_usage' in group.columns else 0,
                'get_memory_usage': group['get_memory_usage'].mean() if 'get_memory_usage' in group.columns else 0,
                'sample_count': len(group)
            })
    
    return pd.DataFrame(stats_list)

def calculate_multi_file_statistics(multi_file_results):
    if not multi_file_results:
        return None, None
    
    df = pd.DataFrame(multi_file_results)
    
    put_results = df[df['test_type'] == 'multi_file_put']
    get_results = df[df['test_type'] == 'multi_file_get']
    
    def process_group(group):
        if group.empty:
            return None
        
        if 'file_type' in group.columns:
            grouped = group.groupby(['file_size_mb', 'file_type'])
        else:
            grouped = group.groupby('file_size_mb')
        
        stats_list = []
        for key, data in grouped:
            if 'file_type' in group.columns:
                size, file_type = key
            else:
                size, file_type = key, 'random'
            
            throughputs = data[data['success']]['total_throughput_mbps']
            avg_file_throughputs = data[data['success']]['avg_file_throughput_mbps']
            
            stats_list.append({
                'file_size_mb': size,
                'file_type': file_type,
                'throughput_mean': throughputs.mean() if len(throughputs) > 0 else 0,
                'throughput_std': throughputs.std() if len(throughputs) > 1 else 0,
                'avg_file_throughput_mean': avg_file_throughputs.mean() if len(avg_file_throughputs) > 0 else 0,
                'success_rate': data['success'].mean(),
                'sample_count': len(data)
            })
        
        return pd.DataFrame(stats_list)
    
    put_stats = process_group(put_results)
    get_stats = process_group(get_results)
    
    return put_stats, get_stats

def save_figure(fig, output_dir, filename, formats=['pdf', 'png'], subdir=None):
    target_dir = output_dir
    if subdir:
        target_dir = os.path.join(output_dir, subdir)
        os.makedirs(target_dir, exist_ok=True)
    for fmt in formats:
        filepath = f'{target_dir}/{filename}.{fmt}'
        fig.savefig(filepath, format=fmt)
    plt.close(fig)

def plot_multi_file_throughput(put_stats, get_stats, output_dir, formats=['pdf', 'png']):
    if put_stats is None and get_stats is None:
        return
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    colors = ['#1f77b4', '#2ca02c', '#d62728', '#ff7f0e', '#9467bd']
    markers = ['o', 's', '^', 'D', 'v']
    
    if put_stats is not None and not put_stats.empty:
        if 'file_type' in put_stats.columns:
            file_types = put_stats['file_type'].unique()
            for i, file_type in enumerate(file_types):
                type_data = put_stats[put_stats['file_type'] == file_type].sort_values('file_size_mb')
                ax.fill_between(type_data['file_size_mb'], 
                               type_data['throughput_mean'] - type_data['throughput_std'],
                               type_data['throughput_mean'] + type_data['throughput_std'],
                               alpha=0.2, color=colors[i % len(colors)])
                ax.errorbar(type_data['file_size_mb'], type_data['throughput_mean'], 
                           yerr=type_data['throughput_std'],
                           marker=markers[i % len(markers)], linestyle='-', 
                           linewidth=1.5, markersize=5, capsize=3,
                           label=f'Multi-PUT ({file_type})', 
                           color=colors[i % len(colors)])
        else:
            put_stats = put_stats.sort_values('file_size_mb')
            ax.fill_between(put_stats['file_size_mb'], 
                           put_stats['throughput_mean'] - put_stats['throughput_std'],
                           put_stats['throughput_mean'] + put_stats['throughput_std'],
                           alpha=0.2, color=colors[0])
            ax.errorbar(put_stats['file_size_mb'], put_stats['throughput_mean'], 
                       yerr=put_stats['throughput_std'],
                       marker='o', linestyle='-', linewidth=1.5, markersize=5, 
                       capsize=3, label='Multi-PUT', color=colors[0])
    
    if get_stats is not None and not get_stats.empty:
        if 'file_type' in get_stats.columns:
            file_types = get_stats['file_type'].unique()
            for i, file_type in enumerate(file_types):
                type_data = get_stats[get_stats['file_type'] == file_type].sort_values('file_size_mb')
                ax.fill_between(type_data['file_size_mb'], 
                               type_data['throughput_mean'] - type_data['throughput_std'],
                               type_data['throughput_mean'] + type_data['throughput_std'],
                               alpha=0.2, color=colors[(i+2) % len(colors)])
                ax.errorbar(type_data['file_size_mb'], type_data['throughput_mean'], 
                           yerr=type_data['throughput_std'],
                           marker=markers[(i+2) % len(markers)], linestyle='--', 
                           linewidth=1.5, markersize=5, capsize=3,
                           label=f'Multi-GET ({file_type})', 
                           color=colors[(i+2) % len(colors)])
        else:
            get_stats = get_stats.sort_values('file_size_mb')
            ax.fill_between(get_stats['file_size_mb'], 
                           get_stats['throughput_mean'] - get_stats['throughput_std'],
                           get_stats['throughput_mean'] + get_stats['throughput_std'],
                           alpha=0.2, color=colors[2])
            ax.errorbar(get_stats['file_size_mb'], get_stats['throughput_mean'], 
                       yerr=get_stats['throughput_std'],
                       marker='s', linestyle='--', linewidth=1.5, markersize=5, 
                       capsize=3, label='Multi-GET', color=colors[2])
    
    ax.set_xlabel('File Size (MB)')
    ax.set_ylabel('Throughput (MB/s)')
    ax.set_title('Multi-File Throughput Performance')
    ax.legend(loc='best', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_xscale('log')
    ax.set_yscale('log')
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'multi_file_throughput', formats)

def plot_single_vs_multi_throughput(stats_df, put_stats, get_stats, output_dir, formats=['pdf', 'png']):
    if put_stats is None and get_stats is None:
        return
    
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    if stats_df is not None and not stats_df.empty:
        if 'file_type' in stats_df.columns:
            merged_single = stats_df.groupby('file_size_mb').agg({
                'put_throughput_mean': 'mean',
                'put_throughput_std': 'mean',
                'get_throughput_mean': 'mean',
                'get_throughput_std': 'mean'
            }).reset_index()
        else:
            merged_single = stats_df
        
        merged_single = merged_single.sort_values('file_size_mb')
        ax.fill_between(merged_single['file_size_mb'], 
                       merged_single['put_throughput_mean'] - merged_single['put_throughput_std'],
                       merged_single['put_throughput_mean'] + merged_single['put_throughput_std'],
                       alpha=0.15, color='#1f77b4')
        ax.errorbar(merged_single['file_size_mb'], merged_single['put_throughput_mean'], 
                   yerr=merged_single['put_throughput_std'],
                   marker='o', linestyle='-', linewidth=1.5, markersize=5, 
                   capsize=3, label='Single-PUT', color='#1f77b4', alpha=0.8)
        
        ax.fill_between(merged_single['file_size_mb'], 
                       merged_single['get_throughput_mean'] - merged_single['get_throughput_std'],
                       merged_single['get_throughput_mean'] + merged_single['get_throughput_std'],
                       alpha=0.15, color='#2ca02c')
        ax.errorbar(merged_single['file_size_mb'], merged_single['get_throughput_mean'], 
                   yerr=merged_single['get_throughput_std'],
                   marker='s', linestyle='-', linewidth=1.5, markersize=5, 
                   capsize=3, label='Single-GET', color='#2ca02c', alpha=0.8)
    
    if put_stats is not None and not put_stats.empty:
        if 'file_type' in put_stats.columns:
            merged_put = put_stats.groupby('file_size_mb').agg({
                'throughput_mean': 'mean',
                'throughput_std': 'mean'
            }).reset_index()
        else:
            merged_put = put_stats
        
        merged_put = merged_put.sort_values('file_size_mb')
        ax.fill_between(merged_put['file_size_mb'], 
                       merged_put['throughput_mean'] - merged_put['throughput_std'],
                       merged_put['throughput_mean'] + merged_put['throughput_std'],
                       alpha=0.15, color='#d62728')
        ax.errorbar(merged_put['file_size_mb'], merged_put['throughput_mean'], 
                   yerr=merged_put['throughput_std'],
                   marker='^', linestyle='--', linewidth=1.5, markersize=5, 
                   capsize=3, label='Multi-PUT', color='#d62728')
    
    if get_stats is not None and not get_stats.empty:
        if 'file_type' in get_stats.columns:
            merged_get = get_stats.groupby('file_size_mb').agg({
                'throughput_mean': 'mean',
                'throughput_std': 'mean'
            }).reset_index()
        else:
            merged_get = get_stats
        
        merged_get = merged_get.sort_values('file_size_mb')
        ax.fill_between(merged_get['file_size_mb'], 
                       merged_get['throughput_mean'] - merged_get['throughput_std'],
                       merged_get['throughput_mean'] + merged_get['throughput_std'],
                       alpha=0.15, color='#ff7f0e')
        ax.errorbar(merged_get['file_size_mb'], merged_get['throughput_mean'], 
                   yerr=merged_get['throughput_std'],
                   marker='D', linestyle='--', linewidth=1.5, markersize=5, 
                   capsize=3, label='Multi-GET', color='#ff7f0e')
    
    ax.set_xlabel('File Size (MB)')
    ax.set_ylabel('Throughput (MB/s)')
    ax.set_title('Single File vs Multi-File Throughput Comparison')
    ax.legend(loc='best', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_xscale('log')
    ax.set_yscale('log')
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'single_vs_multi_throughput', formats)

def plot_throughput_comparison_by_type(stats_df, output_dir, formats=['pdf', 'png']):
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT))
    
    file_types = stats_df['file_type'].unique()
    colors = ['#d62728', '#1f77b4', '#2ca02c', '#ff7f0e', '#9467bd']
    markers = ['o', 's', '^', 'D', 'v']
    
    for i, file_type in enumerate(file_types):
        type_data = stats_df[stats_df['file_type'] == file_type].sort_values('file_size_mb')
        file_sizes = type_data['file_size_mb'].values
        put_throughput = type_data['put_throughput_mean'].values
        get_throughput = type_data['get_throughput_mean'].values
        put_std = type_data['put_throughput_std'].values
        get_std = type_data['get_throughput_std'].values
        
        color = colors[i % len(colors)]
        marker_put = markers[i % len(markers)]
        marker_get = markers[(i + 1) % len(markers)]
        
        ax.fill_between(file_sizes, put_throughput - put_std, put_throughput + put_std,
                       alpha=0.1, color=color)
        ax.errorbar(file_sizes, put_throughput, yerr=put_std, 
                   marker=marker_put, linestyle='-', linewidth=1.5, markersize=5, 
                   capsize=3, label=f'{file_type.upper()} PUT', color=color)
        ax.errorbar(file_sizes, get_throughput, yerr=get_std, 
                   marker=marker_get, linestyle='--', linewidth=1.5, markersize=5, 
                   capsize=3, label=f'{file_type.upper()} GET', color=color, alpha=0.7)
    
    ax.set_xlabel('File Size (MB)')
    ax.set_ylabel('Throughput (MB/s)')
    ax.set_title('DFS Throughput Performance by File Type')
    ax.legend(loc='best', framealpha=0.9, ncol=2)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_xscale('log')
    ax.set_yscale('log')
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'throughput_comparison_by_type', formats)

def plot_put_throughput(stats_df, output_dir, formats=['pdf', 'png']):
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'put_throughput_mean': 'mean',
            'put_throughput_std': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    merged_df = merged_df.sort_values('file_size_mb')
    file_sizes = merged_df['file_size_mb'].values
    put_throughput = merged_df['put_throughput_mean'].values
    put_std = merged_df['put_throughput_std'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.fill_between(x_pos, put_throughput - put_std, put_throughput + put_std,
                   alpha=0.2, color='#1f77b4')
    ax.plot(x_pos, put_throughput, 
           marker='o', linestyle='-', linewidth=2, markersize=6, 
           color='#1f77b4')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Throughput (MB/s)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('PUT Throughput', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_yscale('log')
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    from matplotlib.ticker import FuncFormatter
    def log_formatter(x, pos):
        if x >= 1:
            return f'{int(x)}'
        else:
            return f'{x:.2f}'
    ax.yaxis.set_major_formatter(FuncFormatter(log_formatter))
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'put_throughput', formats, subdir='throughput')


def plot_get_throughput(stats_df, output_dir, formats=['pdf', 'png']):
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'get_throughput_mean': 'mean',
            'get_throughput_std': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    merged_df = merged_df.sort_values('file_size_mb')
    file_sizes = merged_df['file_size_mb'].values
    get_throughput = merged_df['get_throughput_mean'].values
    get_std = merged_df['get_throughput_std'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.fill_between(x_pos, get_throughput - get_std, get_throughput + get_std,
                   alpha=0.2, color='#2ca02c')
    ax.plot(x_pos, get_throughput, 
           marker='s', linestyle='-', linewidth=2, markersize=6, 
           color='#2ca02c')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Throughput (MB/s)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('GET Throughput', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_yscale('log')
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    from matplotlib.ticker import FuncFormatter
    def log_formatter(x, pos):
        if x >= 1:
            return f'{int(x)}'
        else:
            return f'{x:.2f}'
    ax.yaxis.set_major_formatter(FuncFormatter(log_formatter))
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'get_throughput', formats, subdir='throughput')


def plot_put_latency(stats_df, output_dir, formats=['pdf', 'png']):
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'put_latency_mean': 'mean',
            'put_latency_std': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    merged_df = merged_df.sort_values('file_size_mb')
    file_sizes = merged_df['file_size_mb'].values
    put_latency = merged_df['put_latency_mean'].values
    put_std = merged_df['put_latency_std'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.fill_between(x_pos, put_latency - put_std, put_latency + put_std,
                   alpha=0.2, color='#1f77b4')
    ax.plot(x_pos, put_latency, 
           marker='o', linestyle='-', linewidth=2, markersize=6, 
           color='#1f77b4')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Latency (seconds)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('PUT Latency', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'put_latency', formats, subdir='latency')


def plot_get_latency(stats_df, output_dir, formats=['pdf', 'png']):
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'get_latency_mean': 'mean',
            'get_latency_std': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    merged_df = merged_df.sort_values('file_size_mb')
    file_sizes = merged_df['file_size_mb'].values
    get_latency = merged_df['get_latency_mean'].values
    get_std = merged_df['get_latency_std'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.fill_between(x_pos, get_latency - get_std, get_latency + get_std,
                   alpha=0.2, color='#2ca02c')
    ax.plot(x_pos, get_latency, 
           marker='s', linestyle='-', linewidth=2, markersize=6, 
           color='#2ca02c')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Latency (seconds)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('GET Latency', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'get_latency', formats, subdir='latency')

def plot_success_rate(stats_df, output_dir, formats=['pdf', 'png']):
    fig, ax = plt.subplots(figsize=(DOUBLE_COLUMN_WIDTH, DEFAULT_HEIGHT * 0.7))
    
    if 'file_type' in stats_df.columns:
        merged_df = stats_df.groupby('file_size_mb').agg({
            'put_success_rate': 'mean',
            'get_success_rate': 'mean'
        }).reset_index()
    else:
        merged_df = stats_df
    
    merged_df = merged_df.sort_values('file_size_mb')
    file_sizes = merged_df['file_size_mb'].values
    put_success = merged_df['put_success_rate'].values * 100
    get_success = merged_df['get_success_rate'].values * 100
    
    x_pos = np.arange(len(file_sizes))
    
    ax.plot(x_pos, put_success, marker='o', linestyle='-', 
           linewidth=1.5, markersize=5, label='PUT Success Rate', color='#1f77b4')
    ax.plot(x_pos, get_success, marker='s', linestyle='--', 
           linewidth=1.5, markersize=5, label='GET Success Rate', color='#2ca02c')
    
    ax.set_xlabel('File Size')
    ax.set_ylabel('Success Rate (%)')
    ax.set_title('DFS Operation Success Rate vs File Size')
    ax.legend(loc='best', framealpha=0.9)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_ylim([0, 105])
    
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes))
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'success_rate', formats, subdir='success_rate')

def plot_put_cpu_usage(stats_df, output_dir, formats=['pdf', 'png']):
    if 'put_cpu_usage' not in stats_df.columns:
        return
    
    stats_df = stats_df.sort_values('file_size_mb')
    file_sizes = stats_df['file_size_mb'].values
    put_cpu = stats_df['put_cpu_usage'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.plot(x_pos, put_cpu, marker='o', linestyle='-', linewidth=2, markersize=6, color='#1f77b4')
    ax.fill_between(x_pos, 0, put_cpu, alpha=0.2, color='#1f77b4')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('CPU Usage (%)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('PUT CPU Usage', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'put_cpu_usage', formats, subdir='cpu_usage')

def plot_get_cpu_usage(stats_df, output_dir, formats=['pdf', 'png']):
    if 'get_cpu_usage' not in stats_df.columns:
        return
    
    stats_df = stats_df.sort_values('file_size_mb')
    file_sizes = stats_df['file_size_mb'].values
    get_cpu = stats_df['get_cpu_usage'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.plot(x_pos, get_cpu, marker='s', linestyle='-', linewidth=2, markersize=6, color='#2ca02c')
    ax.fill_between(x_pos, 0, get_cpu, alpha=0.2, color='#2ca02c')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('CPU Usage (%)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('GET CPU Usage', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'get_cpu_usage', formats, subdir='cpu_usage')

def plot_put_memory_usage(stats_df, output_dir, formats=['pdf', 'png']):
    if 'put_memory_usage' not in stats_df.columns:
        return
    
    stats_df = stats_df.sort_values('file_size_mb')
    file_sizes = stats_df['file_size_mb'].values
    put_mem = stats_df['put_memory_usage'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.plot(x_pos, put_mem, marker='o', linestyle='-', linewidth=2, markersize=6, color='#1f77b4')
    ax.fill_between(x_pos, 0, put_mem, alpha=0.2, color='#1f77b4')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Memory Usage (%)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('PUT Memory Usage', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'put_memory_usage', formats, subdir='memory_usage')

def plot_get_memory_usage(stats_df, output_dir, formats=['pdf', 'png']):
    if 'get_memory_usage' not in stats_df.columns:
        return
    
    stats_df = stats_df.sort_values('file_size_mb')
    file_sizes = stats_df['file_size_mb'].values
    get_mem = stats_df['get_memory_usage'].values
    
    x_pos = np.arange(len(file_sizes))
    
    fig, ax = plt.subplots(figsize=(SINGLE_COLUMN_WIDTH * 1.5, DEFAULT_HEIGHT))
    
    ax.plot(x_pos, get_mem, marker='s', linestyle='-', linewidth=2, markersize=6, color='#2ca02c')
    ax.fill_between(x_pos, 0, get_mem, alpha=0.2, color='#2ca02c')
    ax.set_xlabel('File Size', fontsize=FONT_SIZE_MEDIUM)
    ax.set_ylabel('Memory Usage (%)', fontsize=FONT_SIZE_MEDIUM)
    ax.set_title('GET Memory Usage', fontsize=FONT_SIZE_LARGE)
    ax.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(format_file_sizes(file_sizes), fontsize=FONT_SIZE_SMALL)
    ax.tick_params(axis='y', labelsize=FONT_SIZE_SMALL)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    plt.tight_layout()
    save_figure(fig, output_dir, 'get_memory_usage', formats, subdir='memory_usage')
    
def generate_ieee_latex_table(stats_df, output_file, system_info=None, statistics=None):
    latex_lines = []
    latex_lines.append("\\begin{table*}[t]")
    latex_lines.append("\\centering")
    latex_lines.append("\\caption{DFS Performance Metrics with 95\\% Confidence Intervals}")
    latex_lines.append("\\label{tab:performance}")
    latex_lines.append("\\begin{tabular}{lcccccc}")
    latex_lines.append("\\toprule")
    latex_lines.append("File Size & Type & PUT Thr. (MB/s) & GET Thr. (MB/s) & PUT Lat. (s) & GET Lat. (s) \\\\")
    latex_lines.append("\\midrule")
    
    for _, row in stats_df.iterrows():
        if 'file_type' in stats_df.columns:
            latex_lines.append(f"{row['file_size_mb']} MB & {row['file_type']} & "
                              f"{row['put_throughput_mean']:.2f} $\\pm$ {row['put_throughput_std']:.2f} & "
                              f"{row['get_throughput_mean']:.2f} $\\pm$ {row['get_throughput_std']:.2f} & "
                              f"{row['put_latency_mean']:.3f} $\\pm$ {row['put_latency_std']:.3f} & "
                              f"{row['get_latency_mean']:.3f} $\\pm$ {row['get_latency_std']:.3f} \\\\")
        else:
            latex_lines.append(f"{row['file_size_mb']} MB & random & "
                              f"{row['put_throughput_mean']:.2f} $\\pm$ {row['put_throughput_std']:.2f} & "
                              f"{row['get_throughput_mean']:.2f} $\\pm$ {row['get_throughput_std']:.2f} & "
                              f"{row['put_latency_mean']:.3f} $\\pm$ {row['put_latency_std']:.3f} & "
                              f"{row['get_latency_mean']:.3f} $\\pm$ {row['get_latency_std']:.3f} \\\\")
    
    latex_lines.append("\\bottomrule")
    latex_lines.append("\\end{tabular}")
    
    if system_info:
        seed = system_info.get('random_seed', 'N/A')
        latex_lines.append(f"\\\\\\small{{Random seed: {seed}, Confidence level: {system_info.get('confidence_level', 0.95)*100:.0f}\\%}}")
    
    latex_lines.append("\\end{table*}")
    
    with open(output_file, 'w') as f:
        f.write('\n'.join(latex_lines))

def generate_acm_latex_table(stats_df, output_file, system_info=None):
    latex_lines = []
    latex_lines.append("\\begin{table}[h]")
    latex_lines.append("\\centering")
    latex_lines.append("\\caption{DFS Performance Results}")
    latex_lines.append("\\label{tab:dfs-perf}")
    latex_lines.append("\\begin{tabular}{lrrrr}")
    latex_lines.append("\\toprule")
    latex_lines.append("\\textbf{Size} & \\textbf{PUT (MB/s)} & \\textbf{GET (MB/s)} & \\textbf{PUT Lat.} & \\textbf{GET Lat.} \\\\")
    latex_lines.append("\\midrule")
    
    for _, row in stats_df.iterrows():
        latex_lines.append(f"{row['file_size_mb']} MB & "
                          f"{row['put_throughput_mean']:.1f} $\\pm$ {row['put_throughput_std']:.1f} & "
                          f"{row['get_throughput_mean']:.1f} $\\pm$ {row['get_throughput_std']:.1f} & "
                          f"{row['put_latency_mean']:.2f}s & "
                          f"{row['get_latency_mean']:.2f}s \\\\")
    
    latex_lines.append("\\bottomrule")
    latex_lines.append("\\end{tabular}")
    latex_lines.append("\\end{table}")
    
    with open(output_file.replace('.tex', '_acm.tex'), 'w') as f:
        f.write('\n'.join(latex_lines))

def generate_summary_table(stats_df, output_file, system_info=None, statistics=None):
    output_dir = os.path.dirname(output_file)
    tables_dir = os.path.join(output_dir, 'tables')
    os.makedirs(tables_dir, exist_ok=True)
    
    base_filename = os.path.basename(output_file)
    tables_output_file = os.path.join(tables_dir, base_filename)
    
    generate_ieee_latex_table(stats_df, tables_output_file, system_info, statistics)
    generate_acm_latex_table(stats_df, tables_output_file, system_info)
    
    if system_info:
        with open(tables_output_file.replace('.tex', '_system_info.txt'), 'w') as f:
            f.write("System Information for Reproducibility\n")
            f.write("=" * 60 + "\n")
            f.write(f"Experiment Timestamp: {system_info.get('timestamp', 'N/A')}\n")
            f.write(f"Random Seed: {system_info.get('random_seed', 'N/A')}\n")
            f.write(f"Warmup Iterations: {system_info.get('warmup_iterations', 0)}\n")
            f.write(f"Cooldown Seconds: {system_info.get('cooldown_seconds', 0)}\n")
            f.write(f"Confidence Level: {system_info.get('confidence_level', 0.95) * 100:.0f}%\n")
            f.write(f"Outliers Removed: {system_info.get('remove_outliers', False)}\n")
            f.write(f"Python Version: {system_info.get('python_version', 'N/A')}\n")
            f.write(f"OS Info: {system_info.get('os_info', 'N/A')}\n")
            f.write(f"CPU Count: {system_info.get('cpu_count', 'N/A')}\n")
            f.write(f"Memory Total (GB): {system_info.get('memory_total_gb', 'N/A'):.2f}\n")
            f.write(f"SciPy Available: {system_info.get('has_scipy', False)}\n")

def generate_plots(results_file, formats=['pdf', 'png']):
    if not Path(results_file).exists():
        print(f"Results file {results_file} not found!")
        return
    
    # 确保输出目录是 tests/performance/plots
    script_dir = Path(__file__).parent
    output_dir = str(script_dir / "plots")
    Path(output_dir).mkdir(exist_ok=True)
    
    setup_academic_style()
    
    print(f"Loading results from {results_file}...")
    results, multi_file_results, system_info, statistics = load_results(results_file)
    stats_df = calculate_statistics(results) if results else pd.DataFrame()
    put_stats, get_stats = calculate_multi_file_statistics(multi_file_results)
    
    print("Generating plots...")
    if not stats_df.empty:
        plot_put_throughput(stats_df, output_dir, formats)
        plot_get_throughput(stats_df, output_dir, formats)
        plot_put_latency(stats_df, output_dir, formats)
        plot_get_latency(stats_df, output_dir, formats)
        plot_success_rate(stats_df, output_dir, formats)
        plot_put_cpu_usage(stats_df, output_dir, formats)
        plot_get_cpu_usage(stats_df, output_dir, formats)
        plot_put_memory_usage(stats_df, output_dir, formats)
        plot_get_memory_usage(stats_df, output_dir, formats)
        
        if 'file_type' in stats_df.columns and len(stats_df['file_type'].unique()) > 1:
            plot_throughput_comparison_by_type(stats_df, output_dir, formats)
    
    if put_stats is not None or get_stats is not None:
        plot_multi_file_throughput(put_stats, get_stats, output_dir, formats)
        if not stats_df.empty:
            plot_single_vs_multi_throughput(stats_df, put_stats, get_stats, output_dir, formats)
    
    if not stats_df.empty:
        generate_summary_table(stats_df, f"{output_dir}/performance_table.tex", system_info, statistics)
    
    print("\n" + "=" * 80)
    print("Performance Summary (Academic Format)")
    print("=" * 80)
    if not stats_df.empty:
        if 'file_type' in stats_df.columns:
            for _, row in stats_df.iterrows():
                print(f"Size: {row['file_size_mb']:>6} MB | Type: {row['file_type']:<8} | "
                      f"PUT: {row['put_throughput_mean']:>6.2f} ± {row['put_throughput_std']:<5.2f} MB/s | "
                      f"GET: {row['get_throughput_mean']:>6.2f} ± {row['get_throughput_std']:<5.2f} MB/s")
        else:
            for _, row in stats_df.iterrows():
                print(f"Size: {row['file_size_mb']:>6} MB | "
                      f"PUT: {row['put_throughput_mean']:>6.2f} ± {row['put_throughput_std']:<5.2f} MB/s | "
                      f"GET: {row['get_throughput_mean']:>6.2f} ± {row['get_throughput_std']:<5.2f} MB/s")
    
    if put_stats is not None or get_stats is not None:
        print("\nMulti-File Performance Summary:")
        print("-" * 80)
        if put_stats is not None and not put_stats.empty:
            for _, row in put_stats.iterrows():
                print(f"Multi-PUT | Size: {row['file_size_mb']:>6} MB | Type: {row['file_type']:<8} | "
                      f"Throughput: {row['throughput_mean']:>6.2f} ± {row['throughput_std']:<5.2f} MB/s")
        if get_stats is not None and not get_stats.empty:
            for _, row in get_stats.iterrows():
                print(f"Multi-GET | Size: {row['file_size_mb']:>6} MB | Type: {row['file_type']:<8} | "
                      f"Throughput: {row['throughput_mean']:>6.2f} ± {row['throughput_std']:<5.2f} MB/s")
    
    print(f"\nAll plots saved to {output_dir}/ directory")
    print(f"LaTeX tables: {output_dir}/performance_table.tex (IEEE), {output_dir}/performance_table_acm.tex (ACM)")
    if system_info:
        print(f"System info: {output_dir}/performance_table_system_info.txt")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        results_file = sys.argv[1]
    else:
        results_file = "performance_results.json"
    
    formats = ['pdf', 'png']
    if len(sys.argv) > 2:
        formats = sys.argv[2].split(',')
    
    generate_plots(results_file, formats)
