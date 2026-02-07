#!/bin/bash

# 分布式文件系统性能测试运行脚本
# 支持学术论文实验的多种配置选项

set -e

echo "=== DFS Performance Test Suite for Academic Research ==="
echo "Current directory: $(pwd)"

# 检查依赖
check_dependencies() {
    echo "Checking dependencies..."
    
    # 检查Python
    if ! command -v python3 &> /dev/null; then
        echo "Error: Python 3 is required but not found"
        exit 1
    fi
    
    # 检查matplotlib
    if ! python3 -c "import matplotlib" &> /dev/null; then
        echo "Warning: matplotlib not found. Install with: pip install matplotlib pandas numpy"
    fi
    
    # 检查psutil
    if ! python3 -c "import psutil" &> /dev/null; then
        echo "Warning: psutil not found. Install with: pip install psutil"
    fi
    
    # 检查DFS是否已编译
    if [ ! -f "/home/lab/dfs/bin/dfc" ] || [ ! -f "/home/lab/dfs/bin/dfs" ]; then
        echo "Error: DFS binaries not found. Please compile first with 'make all'"
        exit 1
    fi
}

# 编译DFS（如果需要）
compile_dfs() {
    echo "Compiling DFS..."
    cd /home/lab/dfs
    make full-clean
    make all
    cd -
}

# 运行测试
run_test() {
    echo "Running performance test..."
    cd /home/lab/dfs/tests
    
    # 根据参数选择测试模式
    if [ "$1" = "--quick" ]; then
        # 快速测试（小文件集，随机数据）
        python3 performance_test.py --sizes 1 5 10 25 50 --iterations 3 --types random
    elif [ "$1" = "--full" ]; then
        # 完整测试（大文件集，多种数据类型）
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 1000 --iterations 5 --types random text binary
    elif [ "$1" = "--academic" ]; then
        # 学术研究标准测试（推荐用于论文）
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 --iterations 5 --types random text binary
    else
        # 标准测试（默认）
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 --iterations 5 --types random
    fi
    
    cd -
}

# 主函数
main() {
    check_dependencies
    
    if [ "$1" = "--compile-only" ]; then
        compile_dfs
        echo "Compilation completed."
        exit 0
    fi
    
    if [ "$1" = "--plot-only" ]; then
        cd /home/lab/dfs/tests
        python3 generate_plots.py
        cd -
        echo "Plots generated."
        exit 0
    fi
    
    # 编译并运行测试
    compile_dfs
    run_test "$@"
    
    echo "=== Test completed successfully! ==="
    echo "Results are in /home/lab/dfs/tests/performance_results.json"
    echo "Plots are in /home/lab/dfs/tests/plots/"
    echo ""
    echo "For academic research, consider using:"
    echo "  make perf-test-academic    # Standard academic test"
    echo "  make perf-test-full       # Comprehensive test with large files"
    echo "  make perf-test-quick      # Quick validation test"
}

# 运行主函数
main "$@"