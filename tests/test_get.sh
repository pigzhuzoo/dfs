#!/bin/bash

# 创建测试文件
echo "test content for get command" > tests/test_get.txt

# 启动客户端并发送命令
{
    sleep 1
    echo "PUT tests/test_get.txt /test_get.txt"
    sleep 3
    echo "GET /test_get.txt tests/downloaded_test_get.txt"
    sleep 3
    echo "EXIT"
} | timeout 20s bin/dfc conf/dfc.conf

# 检查结果
if [ -f tests/downloaded_test_get.txt ]; then
    echo "GET test successful!"
    cat tests/downloaded_test_get.txt
else
    echo "GET test failed!"
    # 显示日志以调试
    echo "=== DFS1 Log ==="
    cat logs/dfs1.log 2>/dev/null || echo "No log"
    echo "=== DFS2 Log ==="
    cat logs/dfs2.log 2>/dev/null || echo "No log"
    echo "=== DFS3 Log ==="
    cat logs/dfs3.log 2>/dev/null || echo "No log"
    echo "=== DFS4 Log ==="
    cat logs/dfs4.log 2>/dev/null || echo "No log"
    
    # 检查服务器上的文件
    echo "=== Server Files ==="
    ls -la server/DFS1/Bob/ 2>/dev/null | grep test_get.txt || echo "DFS1 no test_get.txt"
    ls -la server/DFS2/Bob/ 2>/dev/null | grep test_get.txt || echo "DFS2 no test_get.txt"
    ls -la server/DFS3/Bob/ 2>/dev/null | grep test_get.txt || echo "DFS3 no test_get.txt"
    ls -la server/DFS4/Bob/ 2>/dev/null | grep test_get.txt || echo "DFS4 no test_get.txt"
fi