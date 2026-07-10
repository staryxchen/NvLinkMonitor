CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
NVML_LIB = /usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1
CUDA_INCLUDE = -I/usr/local/cuda/targets/x86_64-linux/include
CUDA_LIB = -L/usr/local/cuda/targets/x86_64-linux/lib -lcudart

BUILD_DIR = build
MONITOR_TARGET = $(BUILD_DIR)/nvlink_monitor
EXAMPLE_TARGET = $(BUILD_DIR)/nvlink_bw_test

MONITOR_SOURCES = monitor/nvlink_monitor.cpp
MONITOR_HEADERS = monitor/nvlink_monitor.h
EXAMPLE_SOURCES = example/nvlink_bw_test.cpp

# Default target
all: $(MONITOR_TARGET) $(EXAMPLE_TARGET)

# Compile the monitor program
$(MONITOR_TARGET): $(MONITOR_SOURCES) $(MONITOR_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CUDA_INCLUDE) -o $(MONITOR_TARGET) $(MONITOR_SOURCES) $(NVML_LIB)

# Compile the example program
$(EXAMPLE_TARGET): $(EXAMPLE_SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CUDA_INCLUDE) -o $(EXAMPLE_TARGET) $(EXAMPLE_SOURCES) $(CUDA_LIB)

# Build only monitor
monitor: $(MONITOR_TARGET)

# Build only example
example: $(EXAMPLE_TARGET)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all monitor example clean 