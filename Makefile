# Makefile for C++ Distributed File System
CXX = clang++
INCLUDE = /usr/lib
LIBS = -lcrypto -lssl
CXXFLAGS = -std=c++17 -g -Wall -Wextra -Iinclude
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

# 源文件
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# 目标可执行文件
DFS_TARGET = $(BINDIR)/dfs
DFC_TARGET = $(BINDIR)/dfc

.PHONY: all clean dfs dfc start run kill clear client dc ds test test-commands test-get test-put test-client full-clean

all: clean dfs dfc start run

# 创建必要目录
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# 编译目标
dfs: $(BINDIR) $(DFS_TARGET)

dfc: $(BINDIR) $(DFC_TARGET)

$(DFS_TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) -o $@ $(filter-out $(OBJDIR)/dfc.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)

$(DFC_TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) -o $@ $(filter-out $(OBJDIR)/dfs.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)

# 编译源文件
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	mkdir -p $(OBJDIR) $(BINDIR)

# 完全清理目标 - 清除所有生成的文件
full-clean: kill
	rm -rf $(OBJDIR) $(BINDIR) logs server/DFS*/*
	mkdir -p $(OBJDIR) $(BINDIR)

# 运行相关目标
kill:
	fuser -k 10001/tcp 10002/tcp 10003/tcp 10004/tcp || true

clear:
	rm -rf server/DFS*/*

start: dfs
	mkdir -p logs
	$(BINDIR)/dfs server/DFS1 10001 &
	$(BINDIR)/dfs server/DFS2 10002 &
	$(BINDIR)/dfs server/DFS3 10003 &
	$(BINDIR)/dfs server/DFS4 10004 &

run:
	tail -f logs/dfs*

# 测试相关目标
test: test-commands test-get test-put test-client

test-commands:
	@echo "Running command tests..."
	@chmod +x tests/test_commands.sh
	@./tests/test_commands.sh

test-get:
	@echo "Running GET tests..."
	@chmod +x tests/test_get.sh
	@./tests/test_get.sh

test-put:
	@echo "Running PUT tests..."
	@python3 tests/test_put.py

test-client:
	@echo "Running client tests..."
	@python3 tests/test_client.py

# 开发调试目标
dc: dfc
	rm -f $(DFC_TARGET)
	$(CXX) -o $(DFC_TARGET) $(filter-out $(OBJDIR)/dfs.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)
	gdb -tui $(DFC_TARGET) --args $(DFC_TARGET) conf/dfc.conf 2> logs/client.log

ds: dfs
	rm -f $(DFS_TARGET)
	$(CXX) -o $(DFS_TARGET) $(filter-out $(OBJDIR)/dfc.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)
	gdb -tui $(DFS_TARGET) --args $(DFS_TARGET) /DFS1 10001

client: dfc
	rm -f $(DFC_TARGET)
	$(CXX) -o $(DFC_TARGET) $(filter-out $(OBJDIR)/dfs.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)
	$(DFC_TARGET) conf/dfc.conf 2> logs/dfc.log