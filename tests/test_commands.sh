#!/bin/bash

# 使用相对路径，不需要cd到绝对路径
# cd /home/lab/dfs_cpp

echo "开始测试DFS命令..."

# 清理之前的测试文件
echo "清理之前的测试文件..."
make clear

# 等待一段时间确保清理完成
sleep 2

# 创建测试文件
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
if [ -f "server/DFS1/Bob/.test_remote_file.txt.1" ] || [ -f "server/DFS2/Bob/.test_remote_file.txt.1" ] || [ -f "server/DFS3/Bob/.test_remote_file.txt.1" ] || [ -f "server/DFS4/Bob/.test_remote_file.txt.1" ] || \
   [ -f "server/DFS1/Bob/.test_remote_file.txt.2" ] || [ -f "server/DFS2/Bob/.test_remote_file.txt.2" ] || [ -f "server/DFS3/Bob/.test_remote_file.txt.2" ] || [ -f "server/DFS4/Bob/.test_remote_file.txt.2" ]; then
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