CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
NVML_LIB = /usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1
CUDA_INCLUDE = -I/usr/local/cuda/targets/x86_64-linux/include
CUDA_LIB = -L/usr/local/cuda/targets/x86_64-linux/lib -lcudart
GTEST_LIBS = -lgtest -lgtest_main -lpthread

BUILD_DIR = build
MONITOR_TARGET = $(BUILD_DIR)/nvlink_monitor
EXAMPLE_TARGET = $(BUILD_DIR)/nvlink_bw_test
TEST_TARGET = $(BUILD_DIR)/run_tests

MONITOR_SOURCES = monitor/nvlink_monitor.cpp monitor/bandwidth_calc.cpp monitor/arg_parser.cpp
MONITOR_HEADERS = monitor/nvlink_monitor.h monitor/bandwidth_calc.h monitor/arg_parser.h
EXAMPLE_SOURCES = example/nvlink_bw_test.cpp example/bw_stats.cpp
EXAMPLE_HEADERS = example/bw_stats.h

TEST_SOURCES = test/test_bandwidth_calc.cpp test/test_arg_parser.cpp test/test_bw_stats.cpp \
               monitor/bandwidth_calc.cpp monitor/arg_parser.cpp example/bw_stats.cpp
TEST_HEADERS = monitor/bandwidth_calc.h monitor/arg_parser.h example/bw_stats.h monitor/nvlink_monitor.h

# Default target
all: $(MONITOR_TARGET) $(EXAMPLE_TARGET)

# Compile the monitor program
$(MONITOR_TARGET): $(MONITOR_SOURCES) $(MONITOR_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CUDA_INCLUDE) -o $(MONITOR_TARGET) $(MONITOR_SOURCES) $(NVML_LIB)

# Compile the example program
$(EXAMPLE_TARGET): $(EXAMPLE_SOURCES) $(EXAMPLE_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CUDA_INCLUDE) -o $(EXAMPLE_TARGET) $(EXAMPLE_SOURCES) $(CUDA_LIB)

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

.PHONY: all monitor example test clean
