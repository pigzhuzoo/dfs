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
    # 检查是否已经编译
    if [ ! -f "bin/dfs" ] || [ ! -f "bin/dfc" ]; then
        make full-clean
        make all
    else
        echo "DFS already compiled, skipping..."
    fi
    cd -
}

# 运行测试
run_test() {
    echo "Running performance test..."
    cd /home/lab/dfs/tests
    
    MULTI_OPTS=""
    if [ "$1" = "--quick" ]; then
        # 快速测试（小文件集，随机数据）
        python3 performance_test.py --sizes 1 5 10 25 50 --iterations 3 --types random
    elif [ "$1" = "--quick-multi" ]; then
        # 快速测试 + 多文件测试
        python3 performance_test.py --sizes 1 5 10 25 50 --iterations 3 --types random --include-multi --num-files 5
    elif [ "$1" = "--full" ]; then
        # 完整测试（大文件集，多种数据类型）
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 1000 --iterations 5 --types random text binary
    elif [ "$1" = "--full-multi" ]; then
        # 完整测试 + 多文件测试
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 1000 --iterations 5 --types random text binary --include-multi --num-files 10
    elif [ "$1" = "--academic" ]; then
        # 学术研究标准测试（推荐用于论文）
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 --iterations 5 --types random text binary
    elif [ "$1" = "--academic-multi" ]; then
        # 学术研究标准测试 + 多文件测试
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 --iterations 5 --types random text binary --include-multi --num-files 10
    elif [ "$1" = "--multi-only" ]; then
        # 仅多文件测试
        shift
        NUM_FILES=${1:-10}
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 --iterations 5 --types random --multi-only --num-files $NUM_FILES
    else
        # 标准测试（默认）
        python3 performance_test.py --sizes 1 5 10 25 50 100 200 500 --iterations 5 --types random
    fi
    
    cd -
}

# 主函数
main() {
    if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
        echo "=== DFS Performance Test Suite Help ==="
        echo "Usage: $0 [OPTION]"
        echo ""
        echo "Options:"
        echo "  --help, -h          Show this help message"
        echo "  --quick             Quick validation test (small files, 3 iterations)"
        echo "  --quick-multi       Quick test + multi-file test"
        echo "  --standard          Standard test (default)"
        echo "  --full              Full test (large files, all types)"
        echo "  --full-multi        Full test + multi-file test"
        echo "  --academic          Academic research standard test"
        echo "  --academic-multi    Academic test + multi-file test"
        echo "  --multi-only [N]    Only run multi-file tests (N files, default 10)"
        echo "  --plot-only         Only generate plots from existing results"
        echo "  --compile-only      Only compile DFS, no tests"
        echo ""
        echo "Examples:"
        echo "  $0 --quick          # Quick check"
        echo "  $0 --academic-multi # Full academic test with multi-file"
        echo "  $0 --multi-only 5   # Only multi-file test with 5 files"
        exit 0
    fi
    
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
    # 启动服务器（如果尚未启动）
    cd /home/lab/dfs
    if ! pgrep -f "bin/dfs server" > /dev/null; then
        echo "Starting DFS servers..."
        make start
    else
        echo "DFS servers already running, skipping startup..."
    fi
    cd -
    
    run_test "$@"
    
    echo "=== Test completed successfully! ==="
    echo "Results are in /home/lab/dfs/tests/performance_results.json"
    echo "Plots are in /home/lab/dfs/tests/plots/"
    echo ""
    echo "For academic research, consider using:"
    echo "  $0 --academic-multi  # Standard academic test with multi-file"
    echo "  $0 --full-multi      # Comprehensive test with multi-file"
    echo "  $0 --quick-multi     # Quick validation with multi-file"
    echo "Use $0 --help for all options"
}

# 运行主函数
main "$@"