# 🚀 NvLinkMonitor

**Real-time per-link NVLink bandwidth monitor + inter-GPU P2P benchmark for NVIDIA multi-GPU systems.** A lightweight C++ toolkit for validating NVLink connectivity and diagnosing link saturation on AI-training and HPC clusters.

[![CI](https://github.com/staryxchen/NvLinkMonitor/actions/workflows/ci.yml/badge.svg)](https://github.com/staryxchen/NvLinkMonitor/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
![Stars](https://img.shields.io/github/stars/staryxchen/NvLinkMonitor?style=social)
![Platform](https://img.shields.io/badge/platform-Linux%20x86__64-blue)

> **Why?** `nvidia-smi` shows GPU utilization but not per-link NVLink throughput. `dcgmi` is heavyweight to deploy. This tool gives you a single dependency-light binary that reports per-link TX/RX bandwidth in real time — plus a built-in inter-GPU copy benchmark (`cudaMemcpy` or `__global__` kernel, unidir or **bidir full-duplex**) to generate load and validate topology. Ideal for bring-up of multi-GPU LLM training nodes.

## ✨ Features

- 📊 **Per-link NVLink bandwidth monitoring** via NVML — the monitor is a single g++ binary, no CUDA toolkit needed
- ⚡ **Inter-GPU P2P benchmark** — copy-engine memcpy or `__global__` kernel, unidirectional or **bidirectional** (aggregate full-duplex bandwidth)
- 🕸️ **All-pairs topology sweep** — test every GPU pair, skip unsupported, print a summary
- 🤖 **Machine-readable output** — `--format csv|json`, status on stderr so stdout stays clean for pipes
- 🎯 **GPU filtering** — `--gpus 0,1,3` to focus on a subset
- 🧪 **81 unit tests**, CI-gated clang-format; pure-logic paths build and test without a GPU

## 🖼️ Demo

```text
$ sudo ./build/nvlink_monitor --verbose
+--- NvLink Monitor (Detailed) --- 2026-07-10 08:29:06 ---+
GPU 0 (18 links) Total RX: 368.4 GiB/s, TX: 368.4 GiB/s
  Link  0 RX:  20.5 GiB/s, TX:  20.5 GiB/s
  Link  1 RX:  20.5 GiB/s, TX:  20.5 GiB/s
  ...

$ ./build/nvlink_bw_test --direction bidir -s 0 -d 1
   • Direction: bidir (aggregate)
   ├─ Average bandwidth: 730.06 GiB/s      # ~2× unidir (368.76) → full-duplex

$ ./build/nvlink_bw_test --all-pairs -i 20 -b 500
═══════════ All-Pairs Summary ═══════════
  GPU 0 ↔ GPU 1: 364.13 GiB/s avg
  GPU 0 ↔ GPU 2: 363.10 GiB/s avg
  ...
```

> 📹 A terminal recording ([asciinema](https://asciinema.org/) / [terminalizer](https://github.com/faressoft/terminalizer)) makes a great first impression — consider embedding one here.

## ⚡ Quick start

```bash
./install-deps.sh          # NVML headers (+ CUDA toolkit only for the benchmark)
make monitor               # monitor only: g++, no CUDA toolkit needed
sudo ./build/nvlink_monitor --verbose
```

---

## Project Structure

```
NvLinkMonitor/
├── monitor/                        # NVLink monitoring tool
│   ├── nvlink_monitor.{cpp,h}      # Monitor class + main()
│   ├── bandwidth_calc.{cpp,h}      # Bandwidth calculation logic (extracted)
│   └── arg_parser.{cpp,h}          # CLI argument parsing (extracted)
├── example/                        # NVLink bandwidth testing tool
│   ├── nvlink_bw_test.cpp          # Bandwidth test main()
│   ├── copy_kernel.{cu,h}          # __global__ copy kernel (compiled by nvcc)
│   ├── bw_stats.{cpp,h}            # Bandwidth stats aggregation (extracted)
│   └── arg_parser.{cpp,h}          # CLI argument parsing (extracted)
├── test/                           # GoogleTest unit tests (no GPU needed)
├── .github/workflows/ci.yml        # CI: format check + unit tests
├── Makefile                        # Build configuration
├── install-deps.sh                 # Dependency installation script
├── .clang-format                   # clang-format style (Google, 4-space)
└── README.md                       # This file
```

## 📋 Prerequisites

Before building the project, you need to install the required dependencies.

### 🔧 Automatic Installation

Run the dependency installation script:

```bash
./install-deps.sh
```

This script will automatically detect your operating system and install the appropriate dependencies.

### 🛠️ Manual Installation

If the automatic script doesn't work for your system, you can install dependencies manually:

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential libnvidia-ml-dev
```

#### CentOS/RHEL/Fedora:
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y nvidia-devel
```

## 🔨 Building

After installing dependencies, build the project:

```bash
# 🏗️ Build both components
make

# 📊 Build only the monitor
make monitor

# ⚡ Build only the example
make example
```

The executables will be created in the `build/` directory:
- `build/nvlink_monitor` - NVLink monitoring tool
- `build/nvlink_bw_test` - NVLink bandwidth test tool

> **Note:** Building `nvlink_bw_test` requires `nvcc` (from the CUDA toolkit) in addition to `g++`, since `example/copy_kernel.cu` contains a `__global__` kernel. The monitor builds with `g++` only. Unit tests (`make test`) do not require `nvcc` or a GPU.

### 🧪 Testing

Unit tests use [GoogleTest](https://github.com/google/googletest) and run **without a GPU** — the test binary links only the extracted pure-logic modules (`bandwidth_calc`, `arg_parser`, `bw_stats`) plus gtest, with no NVML/CUDA library dependencies.

```bash
# Run unit tests (builds the test binary first)
make test

# Apply clang-format in place to all sources
make format

# Fail if any source is not clang-format-clean (CI gate)
make check-format
```

CI (`.github/workflows/ci.yml`) runs `make check-format` and `make test` on ubuntu-22.04 for every push and pull request. The GPU binaries themselves are not built in CI (they require a CUDA toolkit and driver).

## 🧩 Components

### 1. 📊 NVLink Monitor (`monitor/`)

A real-time monitoring tool for NVLink bandwidth and status.

#### 🚀 Basic usage (continuous mode):
```bash
./build/nvlink_monitor
```

#### Continuous monitoring:
```bash
./build/nvlink_monitor --continuous true
```

#### Single monitoring:
```bash
./build/nvlink_monitor --continuous false
```

#### Custom interval (e.g., 0.5 seconds):
```bash
./build/nvlink_monitor --interval 0.5
```

#### Detailed NvLink output:
```bash
./build/nvlink_monitor --verbose
```

#### Combined options:
```bash
./build/nvlink_monitor --continuous false --interval 0.5 --verbose
```

#### Output to file:
```bash
./build/nvlink_monitor -o output.log
./build/nvlink_monitor -v -o detailed.log
```

#### Machine-readable output (CSV / JSON):
```bash
# CSV: header + one row per (gpu, link) per sample. A per-GPU "total" row
# (link_id=total) is always emitted so a GPU with no active links still appears.
./build/nvlink_monitor -f csv -o data.csv

# JSONL: one self-contained JSON object per sample (streaming-friendly for
# continuous mode). Each object carries per-GPU totals and per-link arrays.
./build/nvlink_monitor --format json -o data.jsonl
```

Status/diagnostic messages (GPU discovery, "Starting continuous monitoring",
etc.) are written to **stderr**, so stdout only contains clean CSV/JSON data —
pipe or redirect stdout directly for analysis:
```bash
./build/nvlink_monitor -f csv 2>/dev/null | awk -F, '$4=="total"{print}'
```

#### 📋 Available options:
- `-c, --continuous [true|false]`: Run in continuous mode (default: true)
- `-i, --interval <seconds>`: Set custom monitoring interval in seconds (supports decimals, default: 1.0)
- `-v, --verbose`: Enable detailed NvLink output (shows individual link bandwidth)
- `-o, --output <filename>`: Redirect output to file
- `-f, --format text|csv|json`: Output format (default: text). CSV and JSON are machine-readable; `--verbose` only affects the text path
- `-g, --gpus <id1,id2,...>`: Only monitor the listed GPU indices (e.g. `-g 0,1,3`); default = all GPUs. Out-of-range ids are rejected at startup
- `-h, --help`: Show help information

**Note:** The interval parameter supports decimal values (e.g., 0.5 for 500ms, 0.1 for 100ms). The minimum practical interval is 1 microsecond (0.000001s), but very small intervals may affect system performance.

### 2. ⚡ NVLink Bandwidth Test (`example/`)

A performance testing tool for measuring NVLink bandwidth between GPUs.

#### Basic usage:
```bash
./build/nvlink_bw_test
```

#### ⚙️ Custom parameters:
```bash
./build/nvlink_bw_test -i 200 -b 2000 -s 0 -d 1
```

#### 🧬 Kernel-based copy (instead of cudaMemcpy):
```bash
# Use a __global__ copy kernel issuing vectorized load/stores over P2P.
# Produces NVLink traffic just like the default memcpy path, but exercises
# the SMs rather than the copy engine.
./build/nvlink_bw_test --mode kernel -i 100 -b 1000 -s 0 -d 1
```

#### 🔁 Bidirectional (full-duplex aggregate bandwidth):
```bash
# Issue src->dst and dst->src concurrently on per-GPU streams; reports
# aggregate bandwidth = 2 * size / elapsed (both directions share the
# wall-clock window). Latency stays per-direction.
./build/nvlink_bw_test --direction bidir -i 100 -b 1000 -s 0 -d 1
```

#### 🕸️ All-pairs topology sweep:
```bash
# Test every i<j GPU pair (ignores -s/-d), skipping pairs without P2P.
# Prints per-pair results and a compact summary at the end. Combines with
# --direction bidir for a full-duplex topology sweep.
./build/nvlink_bw_test --all-pairs -i 20 -b 500
./build/nvlink_bw_test --all-pairs --direction bidir -i 20 -b 500
```

#### 📋 Available options:
- `-i, --iterations NUM`: Number of iterations (default: 100)
- `-b, --buffer-size NUM`: Buffer size in MB (default: 1000)
- `-s, --src-gpu NUM`: Source GPU ID (default: 0)
- `-d, --dst-gpu NUM`: Destination GPU ID (default: 1)
- `-m, --mode memcpy|kernel`: Copy method (default: memcpy). `memcpy` uses `cudaMemcpyDeviceToDevice` (copy engine DMA); `kernel` launches a vectorized `__global__` copy kernel whose load/stores traverse NVLink via P2P peer access.
- `--direction unidir|bidir`: unidirectional (default) or bidirectional. `bidir` issues both directions concurrently and reports aggregate full-duplex bandwidth (2× size / elapsed).
- `--all-pairs`: Sweep all i<j GPU pairs (ignores `-s/-d`); skips pairs without P2P and prints a summary. Composes with `--direction`.
- `-h, --help`: Show help message

#### Example:
```bash
./build/nvlink_bw_test -i 200 -b 2000 -s 0 -d 1 -m kernel
./build/nvlink_bw_test --all-pairs --direction bidir -i 20 -b 500
```

## 🔧 Technical Details

### 📦 Dependencies:
- **NVML (NVIDIA Management Library)**: For GPU monitoring and NVLink data access
- **CUDA Runtime**: For GPU memory operations and device management
- **C++11**: For modern C++ features

### ⚙️ Supported Operations:
- **NVLink Monitoring**: Real-time bandwidth monitoring across all NVLink links
- **Peer-to-Peer Access**: Automatic P2P access setup between GPUs
- **Memory Operations**: Device-to-device memory copies for bandwidth testing
- **Performance Measurement**: High-precision timing and bandwidth calculation

## 🧹 Cleaning

To clean build artifacts:

```bash
make clean
```

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## ⭐ Star This Project!

If this toolkit helped you squeeze every last bit of bandwidth out of your NVLink connections (or just saved you from pulling your hair out debugging GPU-to-GPU transfers), please give it a star! 🚀