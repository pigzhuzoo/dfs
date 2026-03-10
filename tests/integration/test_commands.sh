#!/bin/bash

echo "开始测试DFS命令..."

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
echo "This is a test file for PUT command" > tests/test_put.txt
echo "This is another test file" > tests/test_another.txt

# 测试MKDIR命令
echo "测试MKDIR命令..."
{
    sleep 1
    echo "MKDIR test_folder"
    sleep 2
    echo "EXIT"
} | timeout 10s bin/dfc conf/dfc.conf

# 检查目录是否创建成功
if [ -d "server/DFS1/Bob/test_folder" ] || [ -d "server/DFS2/Bob/test_folder" ] || [ -d "server/DFS3/Bob/test_folder" ] || [ -d "server/DFS4/Bob/test_folder" ]; then
    echo "MKDIR测试成功！"
else
    echo "MKDIR测试失败！"
fi

# 测试PUT命令
echo "测试PUT命令..."
{
    sleep 1
    echo "PUT tests/test_put.txt test_remote_file.txt"
    sleep 3
    echo "EXIT"
} | timeout 15s bin/dfc conf/dfc.conf

# 检查文件是否上传成功
if [ -f "server/DFS1/Bob/.test_remote_file.txt.0" ] || [ -f "server/DFS2/Bob/.test_remote_file.txt.0" ] || [ -f "server/DFS3/Bob/.test_remote_file.txt.0" ] || [ -f "server/DFS4/Bob/.test_remote_file.txt.0" ]; then
    echo "PUT测试成功！"
else
    echo "PUT测试失败！"
fi

# 测试LIST命令
echo "测试LIST命令..."
{
    sleep 1
    echo "LIST /"
    sleep 2
    echo "EXIT"
} | timeout 10s bin/dfc conf/dfc.conf

# 测试GET命令
echo "测试GET命令..."
{
    sleep 1
    echo "GET test_remote_file.txt tests/downloaded_file.txt"
    sleep 3
    echo "EXIT"
} | timeout 15s bin/dfc conf/dfc.conf

# 检查文件是否下载成功
if [ -f "tests/downloaded_file.txt" ]; then
    echo "GET测试成功！"
    echo "下载的文件内容："
    cat tests/downloaded_file.txt
else
    echo "GET测试失败！"
fi

# 清理测试文件
echo "清理测试文件..."
rm -f tests/test_put.txt tests/test_another.txt tests/downloaded_file.txt

echo "命令测试完成！"