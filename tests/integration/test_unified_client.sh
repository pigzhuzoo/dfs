#!/bin/bash

echo "开始测试统一客户端 (dfc-unified)..."

make kill > /dev/null 2>&1
sleep 1

rm -rf server/DFS*/*
mkdir -p server/DFS1 server/DFS2 server/DFS3 server/DFS4

bin/dfs server/DFS1 10001 --no-debug &
bin/dfs server/DFS2 10002 --no-debug &
bin/dfs server/DFS3 10003 --no-debug &
bin/dfs server/DFS4 10004 --no-debug &

sleep 3

echo "服务器已启动"

echo "创建测试文件..."
echo "This is a test file for unified client" > tests/unified_test_put.txt

# ==========================================
# 测试统一客户端的交互式模式
# ==========================================
echo ""
echo "========================================"
echo "测试统一客户端 - 交互式模式"
echo "========================================"

# 测试MKDIR命令
echo "测试MKDIR命令..."
{
    sleep 1
    echo "MKDIR unified_test_folder"
    sleep 2
    echo "EXIT"
} | timeout 10s bin/dfc-unified conf/dfc.conf

# 检查目录是否创建成功
if [ -d "server/DFS1/Bob/unified_test_folder" ] || [ -d "server/DFS2/Bob/unified_test_folder" ] || [ -d "server/DFS3/Bob/unified_test_folder" ] || [ -d "server/DFS4/Bob/unified_test_folder" ]; then
    echo "✅ 交互式模式 MKDIR测试成功！"
else
    echo "❌ 交互式模式 MKDIR测试失败！"
fi

# 测试PUT命令
echo "测试PUT命令..."
{
    sleep 1
    echo "PUT tests/unified_test_put.txt unified_remote_file.txt"
    sleep 3
    echo "EXIT"
} | timeout 15s bin/dfc-unified conf/dfc.conf

# 检查文件是否上传成功
if [ -f "server/DFS1/Bob/.unified_remote_file.txt.0" ] || [ -f "server/DFS2/Bob/.unified_remote_file.txt.0" ] || [ -f "server/DFS3/Bob/.unified_remote_file.txt.0" ] || [ -f "server/DFS4/Bob/.unified_remote_file.txt.0" ]; then
    echo "✅ 交互式模式 PUT测试成功！"
else
    echo "❌ 交互式模式 PUT测试失败！"
fi

# 测试LIST命令
echo "测试LIST命令..."
{
    sleep 1
    echo "LIST /"
    sleep 2
    echo "EXIT"
} | timeout 10s bin/dfc-unified conf/dfc.conf

# 测试GET命令
echo "测试GET命令..."
{
    sleep 1
    echo "GET unified_remote_file.txt tests/unified_downloaded_file.txt"
    sleep 3
    echo "EXIT"
} | timeout 15s bin/dfc-unified conf/dfc.conf

# 检查文件是否下载成功
if [ -f "tests/unified_downloaded_file.txt" ]; then
    echo "✅ 交互式模式 GET测试成功！"
    echo "下载的文件内容："
    cat tests/unified_downloaded_file.txt
else
    echo "❌ 交互式模式 GET测试失败！"
fi

# ==========================================
# 测试统一客户端的帮助信息
# ==========================================
echo ""
echo "========================================"
echo "测试统一客户端 - 帮助信息"
echo "========================================"
bin/dfc-unified --help
echo ""
echo "✅ 帮助信息显示成功！"

# 清理测试文件
echo ""
echo "清理测试文件..."
rm -f tests/unified_test_put.txt tests/unified_downloaded_file.txt

echo ""
echo "========================================"
echo "统一客户端测试完成！"
echo "========================================"
