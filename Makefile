CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
NVML_LIB = /usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1
CUDA_INCLUDE = -I/usr/local/cuda/targets/x86_64-linux/include
CUDA_LIB = -L/usr/local/cuda/targets/x86_64-linux/lib -lcudart
GTEST_LIBS = -lgtest -lgtest_main -lpthread

# nvcc for compiling .cu kernel sources (only used by the example binary).
# C++14 is the minimum device standard supported by CUDA 12+. The -gencode list
# covers V100 (sm_70), A100 (sm_80), H100 (sm_90), plus compute_90 PTX for
# forward compatibility with newer GPUs via JIT.
NVCC = nvcc
NVCC_ARCH = -gencode arch=compute_70,code=sm_70 \
            -gencode arch=compute_80,code=sm_80 \
            -gencode arch=compute_90,code=sm_90 \
            -gencode arch=compute_90,code=compute_90
NVCC_FLAGS = -std=c++14 -O2 $(NVCC_ARCH)

BUILD_DIR = build
MONITOR_TARGET = $(BUILD_DIR)/nvlink_monitor
EXAMPLE_TARGET = $(BUILD_DIR)/nvlink_bw_test
TEST_TARGET = $(BUILD_DIR)/run_tests

MONITOR_SOURCES = monitor/nvlink_monitor.cpp monitor/bandwidth_calc.cpp monitor/arg_parser.cpp
MONITOR_HEADERS = monitor/nvlink_monitor.h monitor/bandwidth_calc.h monitor/arg_parser.h
EXAMPLE_SOURCES = example/nvlink_bw_test.cpp example/bw_stats.cpp example/arg_parser.cpp
EXAMPLE_HEADERS = example/bw_stats.h example/arg_parser.h example/copy_kernel.h
EXAMPLE_KERNEL = example/copy_kernel.cu
EXAMPLE_KERNEL_OBJ = $(BUILD_DIR)/copy_kernel.o

TEST_SOURCES = test/test_bandwidth_calc.cpp test/test_arg_parser.cpp test/test_bw_stats.cpp \
               test/test_bw_test_args.cpp \
               monitor/bandwidth_calc.cpp monitor/arg_parser.cpp example/bw_stats.cpp example/arg_parser.cpp
TEST_HEADERS = monitor/bandwidth_calc.h monitor/arg_parser.h example/bw_stats.h example/arg_parser.h monitor/nvlink_monitor.h

# Sources checked by clang-format (all C++ sources and headers, incl. .cu)
FORMAT_SOURCES = $(MONITOR_SOURCES) $(MONITOR_HEADERS) \
                 $(EXAMPLE_SOURCES) $(EXAMPLE_HEADERS) $(EXAMPLE_KERNEL) test/*.cpp

# Default target
all: $(MONITOR_TARGET) $(EXAMPLE_TARGET)

# Compile the monitor program
$(MONITOR_TARGET): $(MONITOR_SOURCES) $(MONITOR_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CUDA_INCLUDE) -o $(MONITOR_TARGET) $(MONITOR_SOURCES) $(NVML_LIB)

# Compile the example program (g++ links nvcc-compiled kernel object)
$(EXAMPLE_TARGET): $(EXAMPLE_SOURCES) $(EXAMPLE_HEADERS) $(EXAMPLE_KERNEL_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CUDA_INCLUDE) -o $(EXAMPLE_TARGET) \
	    $(EXAMPLE_SOURCES) $(EXAMPLE_KERNEL_OBJ) $(CUDA_LIB)

# Compile the CUDA copy kernel with nvcc (only this file needs nvcc)
$(EXAMPLE_KERNEL_OBJ): $(EXAMPLE_KERNEL) example/copy_kernel.h
	@mkdir -p $(BUILD_DIR)
	$(NVCC) $(NVCC_FLAGS) -o $(EXAMPLE_KERNEL_OBJ) -c $(EXAMPLE_KERNEL)

# Compile and run unit tests (no GPU/NVML/CUDA required to link)
$(TEST_TARGET): $(TEST_SOURCES) $(TEST_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. $(CUDA_INCLUDE) -o $(TEST_TARGET) $(TEST_SOURCES) $(GTEST_LIBS)

# Build only monitor
monitor: $(MONITOR_TARGET)

# Build only example
example: $(EXAMPLE_TARGET)

# Build and run unit tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Apply clang-format in place to all sources
format:
	clang-format -i $(FORMAT_SOURCES)

# Fail if any source is not clang-format-clean (for CI)
check-format:
	@clang-format --dry-run --Werror $(FORMAT_SOURCES)

.PHONY: all monitor example test format check-format clean
