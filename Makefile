# Makefile for C++ Distributed File System
CXX = clang++
LIBS = -lcrypto -lssl -lstdc++fs -pthread
CXXFLAGS = -std=c++17 -g -Wall -Wextra -Iinclude -Iinclude/common -Iinclude/crypto -Iinclude/network -Iinclude/client -Iinclude/server

# FPGA Support (enable with: make USE_FPGA=1)
ifdef USE_FPGA
    XRT_PATH ?= /opt/xilinx/xrt
    XCLBIN_PATH ?= /home/zhuzh/data/test/aes-hls-fpga/build/aes256.hw.xclbin
    
    CXXFLAGS += -DUSE_FPGA -DXCLBIN_PATH=\"$(XCLBIN_PATH)\"
    CXXFLAGS += -I$(XRT_PATH)/include
    LIBS += -L$(XRT_PATH)/lib -lxilinxopencl -lOpenCL
    LIBS += -Wl,-rpath,$(XRT_PATH)/lib
endif

# Source directories
SRCDIRS = src src/common src/crypto src/network src/client src/server
OBJDIR = obj
BINDIR = bin

# Find all source files
SOURCES = $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
OBJECTS = $(SOURCES:src/%.cpp=$(OBJDIR)/%.o)

DFS_TARGET = $(BINDIR)/dfs
DFC_TARGET = $(BINDIR)/dfc
DFC_UNIFIED_TARGET = $(BINDIR)/dfc-unified

.PHONY: all clean dfs dfc dfc-unified start kill clear test test-commands test-get test-put test-encryption test-crypto test-unified perf-test perf-test-quick perf-test-full perf-test-plots client multi-tenant-test dfs-fpga dfc-fpga perf-test-fpga perf-test-compare

all: clean dfs dfc dfc-unified start

$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/common $(OBJDIR)/crypto $(OBJDIR)/network $(OBJDIR)/client $(OBJDIR)/server

$(BINDIR):
	mkdir -p $(BINDIR)

dfs: $(BINDIR) $(DFS_TARGET)

dfc: $(BINDIR) $(DFC_TARGET)

$(DFS_TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) -o $@ $(filter-out $(OBJDIR)/client/dfc.o $(OBJDIR)/client/dfc_unified.o $(OBJDIR)/client/dfs_client_service.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)

$(DFC_TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) -o $@ $(filter-out $(OBJDIR)/server/dfs.o $(OBJDIR)/client/dfc_unified.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)

dfc-unified: $(BINDIR) $(DFC_UNIFIED_TARGET)

$(DFC_UNIFIED_TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) -o $@ $(filter-out $(OBJDIR)/server/dfs.o $(OBJDIR)/client/dfc.o, $(OBJECTS)) $(CXXFLAGS) $(LIBS)

# FPGA-enabled builds
dfs-fpga:
	@echo "Building DFS with FPGA support..."
	$(MAKE) USE_FPGA=1 dfs

dfc-fpga:
	@echo "Building DFC with FPGA support..."
	$(MAKE) USE_FPGA=1 dfc

# Compile pattern for all source files
$(OBJDIR)/%.o: src/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f tests/integration/*.json tests/integration/*.bin tests/integration/downloaded_*
	rm -f tests/performance/plots/*.bin tests/performance/plots/*.png tests/performance/plots/*.pdf tests/performance/plots/*.tex tests/performance/plots/*.txt
	mkdir -p $(OBJDIR) $(BINDIR) tests/performance/plots
	mkdir -p $(OBJDIR)/common $(OBJDIR)/crypto $(OBJDIR)/network $(OBJDIR)/client $(OBJDIR)/server

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

start-no-debug: dfs
	mkdir -p logs
	$(BINDIR)/dfs server/DFS1 10001 --no-debug &
	$(BINDIR)/dfs server/DFS2 10002 --no-debug &
	$(BINDIR)/dfs server/DFS3 10003 --no-debug &
	$(BINDIR)/dfs server/DFS4 10004 --no-debug &

test: test-commands test-get test-put test-encryption test-unified

test-commands:
	@echo "Running command tests..."
	@chmod +x tests/integration/test_commands.sh
	@./tests/integration/test_commands.sh

test-unified:
	@echo "Running unified client tests..."
	@chmod +x tests/integration/test_unified_client.sh
	@./tests/integration/test_unified_client.sh

test-get:
	@echo "Running GET tests..."
	@chmod +x tests/integration/test_get.sh
	@./tests/integration/test_get.sh

test-put:
	@echo "Running PUT tests..."
	@python3 tests/integration/test_put.py

test-encryption:
	@echo "Running encryption tests..."
	@chmod +x tests/integration/test_encryption.sh
	@./tests/integration/test_encryption.sh

test-crypto:
	@echo "Running encryption algorithm tests..."
	$(CXX) -std=c++17 -g -Wall -Wextra -Iinclude -Iinclude/common -Iinclude/crypto -Iinclude/network -Iinclude/client -Iinclude/server -o bin/test_crypto tests/unit/test_crypto.cpp src/crypto/crypto_utils.cpp src/crypto/fpga_aes.cpp src/common/utils.cpp src/common/logger.cpp $(LIBS)
	@./bin/test_crypto

perf-test: perf-test-full

perf-test-quick:
	@echo "Running quick performance test..."
	@make kill
	@make start-no-debug
	@sleep 2
	@cd tests/performance && python3 performance_test.py --sizes 1 4 16 --iterations 3 --warmup 1 --seed 42 --output plots/performance_results.json

perf-test-full:
	@echo "Running full performance test..."
	@make kill
	@make start-no-debug
	@sleep 2
	@cd tests/performance && python3 performance_test.py --sizes 0.004 0.016 0.064 0.256 1 4 16 --iterations 5 --warmup 2 --cooldown 1 --seed 42 --confidence-level 0.95 --output plots/performance_results.json

perf-test-plots:
	@echo "Generating performance plots from existing results..."
	@cd tests/performance && python3 generate_plots.py plots/performance_results.json

perf-test-fpga:
	@echo "Running FPGA performance test..."
	@make kill
	$(MAKE) USE_FPGA=1 start-no-debug
	@sleep 2
	@cd tests/performance && python3 performance_test.py --sizes 0.004 0.016 0.064 0.256 1 4 16 --iterations 5 --warmup 2 --cooldown 1 --seed 42 --confidence-level 0.95 --fpga --output plots/performance_results_fpga.json

perf-test-compare:
	@echo "Running comparative performance test (CPU vs FPGA)..."
	@echo "=== Phase 1: CPU Baseline (AES-256-ECB) ==="
	@make kill
	@make clean
	@make dfs dfc
	@sed -i 's/EncryptionType:.*/EncryptionType: AES_256_ECB/' conf/dfc.conf
	@make start-no-debug
	@sleep 2
	@cd tests/performance && python3 performance_test.py --sizes 0.004 0.016 0.064 0.256 1 4 16 --iterations 5 --warmup 2 --cooldown 1 --seed 42 --confidence-level 0.95 --output plots/performance_results_cpu.json
	@echo "=== Phase 2: FPGA Accelerated (AES-256-FPGA) ==="
	@make kill
# 	@make clean
	@make USE_FPGA=1 dfs dfc
	@sed -i 's/EncryptionType:.*/EncryptionType: AES_256_FPGA/' conf/dfc.conf
	@make USE_FPGA=1 start-no-debug
	@sleep 2
	@cd tests/performance && python3 performance_test.py --sizes 0.004 0.016 0.064 0.256 1 4 16 --iterations 5 --warmup 2 --cooldown 1 --seed 42 --confidence-level 0.95 --output plots/performance_results_fpga.json
	@echo "=== Phase 3: Generating All Plots ==="
	@cd tests/performance && python3 generate_plots.py plots/performance_results_cpu.json
	@cd tests/performance && python3 generate_plots.py plots/performance_results_fpga.json
	@echo "=== Phase 4: Generating Comparison Report ==="
	@cd tests/performance && python3 compare_performance.py plots/performance_results_cpu.json plots/performance_results_fpga.json comparison_report.json
	@echo "=== All tests completed! Results saved to tests/performance/plots/ ==="

multi-tenant-test:
	@echo "Running multi-tenant performance test..."
	@make kill
	@make start-no-debug
	@sleep 2
	@cd tests/performance && python3 multi_tenant_test.py --max-tenants 4 --iterations 2 --file-size 0.1 --acceptable-latency 3000 --max-connections 10 --plots --output plots/multi_tenant_results.json --cooldown 3

client: dfc
	@echo "Starting interactive DFS client..."
	@$(DFC_TARGET) conf/dfc.conf
