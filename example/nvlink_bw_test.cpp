#include <cuda_runtime.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

#include "arg_parser.h"
#include "bw_stats.h"
#include "copy_kernel.h"

static bool checkCudaErrorReturn(cudaError_t result, const char* message) {
    if (result != cudaSuccess) {
        std::cerr << message << " (Error code: " << result << " - "
                  << cudaGetErrorString(result) << ")" << std::endl;
        return false;
    }
    return true;
}

bool checkP2PSupport(int src_gpu, int dst_gpu) {
    int accessSupported;
    cudaError_t err = cudaDeviceGetP2PAttribute(
        &accessSupported, cudaDevP2PAttrAccessSupported, src_gpu, dst_gpu);

    if (!checkCudaErrorReturn(err, "Failed to check P2P support")) {
        return false;
    }

    if (accessSupported) {
        std::cout << "✓ P2P access supported between GPU " << src_gpu
                  << " and GPU " << dst_gpu << std::endl;
        return true;
    } else {
        std::cout << "✗ P2P access not supported between GPU " << src_gpu
                  << " and GPU " << dst_gpu << std::endl;
        return false;
    }
}

bool enableP2PAccess(int src_gpu, int dst_gpu) {
    cudaError_t err = cudaSetDevice(src_gpu);
    if (!checkCudaErrorReturn(err, "Failed to set source device")) {
        return false;
    }

    err = cudaDeviceEnablePeerAccess(dst_gpu, 0);
    if (err != cudaSuccess && err != cudaErrorPeerAccessAlreadyEnabled) {
        if (!checkCudaErrorReturn(
                err,
                "Failed to enable P2P access from source to destination GPU")) {
            return false;
        }
    }

    err = cudaSetDevice(dst_gpu);
    if (!checkCudaErrorReturn(err, "Failed to set destination device")) {
        return false;
    }

    err = cudaDeviceEnablePeerAccess(src_gpu, 0);
    if (err != cudaSuccess && err != cudaErrorPeerAccessAlreadyEnabled) {
        if (!checkCudaErrorReturn(
                err,
                "Failed to enable P2P access from destination to source GPU")) {
            return false;
        }
    }

    std::cout << "✓ P2P access enabled between GPUs" << std::endl;
    return true;
}

void printGPUInfo(int src_gpu, int dst_gpu) {
    int deviceCount;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (!checkCudaErrorReturn(err, "Failed to get device count")) {
        return;
    }

    std::cout << "GPU Information:" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
                 "━━━━━━━"
              << std::endl;

    // Print source GPU information
    if (src_gpu < deviceCount) {
        cudaDeviceProp prop;
        err = cudaGetDeviceProperties(&prop, src_gpu);
        if (checkCudaErrorReturn(err,
                                 "Failed to get source device properties")) {
            std::cout << "Source GPU " << src_gpu << ": " << prop.name
                      << std::endl;
            std::cout << "   ├─ Memory: "
                      << prop.totalGlobalMem / (1024 * 1024 * 1024) << " GB"
                      << std::endl;
            std::cout << "   ├─ Compute Capability: " << prop.major << "."
                      << prop.minor << std::endl;
            std::cout << "   ├─ Max Threads per Block: "
                      << prop.maxThreadsPerBlock << std::endl;
            std::cout << "   ├─ Max Threads per SM: "
                      << prop.maxThreadsPerMultiProcessor << std::endl;
            std::cout << "   └─ Number of SMs: " << prop.multiProcessorCount
                      << std::endl;
        }
    }

    // Print destination GPU information
    if (dst_gpu < deviceCount) {
        cudaDeviceProp prop;
        err = cudaGetDeviceProperties(&prop, dst_gpu);
        if (checkCudaErrorReturn(
                err, "Failed to get destination device properties")) {
            std::cout << "Destination GPU " << dst_gpu << ": " << prop.name
                      << std::endl;
            std::cout << "   ├─ Memory: "
                      << prop.totalGlobalMem / (1024 * 1024 * 1024) << " GB"
                      << std::endl;
            std::cout << "   ├─ Compute Capability: " << prop.major << "."
                      << prop.minor << std::endl;
            std::cout << "   ├─ Max Threads per Block: "
                      << prop.maxThreadsPerBlock << std::endl;
            std::cout << "   ├─ Max Threads per SM: "
                      << prop.maxThreadsPerMultiProcessor << std::endl;
            std::cout << "   └─ Number of SMs: " << prop.multiProcessorCount
                      << std::endl;
        }
    }
    std::cout << std::endl;
}

double measureCopyTime(void* dst_ptr, void* src_ptr, size_t size_bytes,
                       int src_gpu, cudaMemcpyKind copy_kind) {
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set device for copy measurement")) {
        return -1.0;
    }

    cudaEvent_t start, stop;
    if (!checkCudaErrorReturn(cudaEventCreate(&start),
                              "Failed to create start event")) {
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaEventCreate(&stop),
                              "Failed to create stop event")) {
        cudaEventDestroy(start);
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaDeviceSynchronize(),
                              "Failed to synchronize device")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaEventRecord(start),
                              "Failed to record start event")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    if (!checkCudaErrorReturn(
            cudaMemcpy(dst_ptr, src_ptr, size_bytes, copy_kind),
            "Copy failed")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaEventRecord(stop),
                              "Failed to record stop event")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaEventSynchronize(stop),
                              "Failed to synchronize stop event")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    float time_ms;
    if (!checkCudaErrorReturn(cudaEventElapsedTime(&time_ms, start, stop),
                              "Failed to get elapsed time")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    return time_ms;
}

// Times a kernel-based copy (launchCopyKernel) with cudaEvent start/stop,
// mirroring measureCopyTime. The kernel is launched on src_gpu and writes to
// dst_ptr over P2P/NVLink. Returns elapsed ms, or -1.0 on error.
double measureCopyTimeKernel(void* dst_ptr, void* src_ptr, size_t size_bytes,
                             int src_gpu) {
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set device for kernel copy")) {
        return -1.0;
    }

    cudaEvent_t start, stop;
    if (!checkCudaErrorReturn(cudaEventCreate(&start),
                              "Failed to create start event")) {
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaEventCreate(&stop),
                              "Failed to create stop event")) {
        cudaEventDestroy(start);
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaDeviceSynchronize(),
                              "Failed to synchronize device")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaEventRecord(start),
                              "Failed to record start event")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    cudaError_t err = launchCopyKernel(dst_ptr, src_ptr, size_bytes, src_gpu);
    if (err != cudaSuccess) {
        std::cerr << "Kernel launch failed (Error code: " << err << " - "
                  << cudaGetErrorString(err) << ")" << std::endl;
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    if (!checkCudaErrorReturn(cudaEventRecord(stop),
                              "Failed to record stop event")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    // Execution errors (e.g. illegal memory access) surface here.
    if (!checkCudaErrorReturn(cudaEventSynchronize(stop),
                              "Kernel copy failed")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    float time_ms;
    if (!checkCudaErrorReturn(cudaEventElapsedTime(&time_ms, start, stop),
                              "Failed to get elapsed time")) {
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return -1.0;
    }

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    return time_ms;
}

void testCopyPerformance(int src_gpu, int dst_gpu, size_t buffer_size_mb,
                         int iterations, CopyMode mode) {
    std::cout << "Buffer size: " << buffer_size_mb
              << " MB | Iterations: " << iterations << std::endl;

    void *src_ptr, *dst_ptr;
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set source device for allocation")) {
        return;
    }

    if (!checkCudaErrorReturn(
            cudaMalloc(&src_ptr, buffer_size_mb * 1024 * 1024),
            "Failed to allocate source memory")) {
        return;
    }

    if (!checkCudaErrorReturn(
            cudaSetDevice(dst_gpu),
            "Failed to set destination device for allocation")) {
        cudaFree(src_ptr);
        return;
    }

    if (!checkCudaErrorReturn(
            cudaMalloc(&dst_ptr, buffer_size_mb * 1024 * 1024),
            "Failed to allocate destination memory")) {
        cudaFree(src_ptr);
        return;
    }

    // Initialize source buffer
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set source device for memset")) {
        cudaFree(src_ptr);
        cudaFree(dst_ptr);
        return;
    }

    if (!checkCudaErrorReturn(
            cudaMemset(src_ptr, 0x42, buffer_size_mb * 1024 * 1024),
            "Failed to initialize source buffer")) {
        cudaFree(src_ptr);
        cudaFree(dst_ptr);
        return;
    }

    if (!checkCudaErrorReturn(cudaDeviceSynchronize(),
                              "Failed to synchronize device after memset")) {
        cudaFree(src_ptr);
        cudaFree(dst_ptr);
        return;
    }

    std::vector<double> copy_times;

    for (int i = 0; i < iterations; i++) {
        double copy_time;
        if (mode == CopyMode::Kernel) {
            copy_time = measureCopyTimeKernel(
                dst_ptr, src_ptr, buffer_size_mb * 1024 * 1024, src_gpu);
        } else {
            copy_time =
                measureCopyTime(dst_ptr, src_ptr, buffer_size_mb * 1024 * 1024,
                                src_gpu, cudaMemcpyDeviceToDevice);
        }

        if (copy_time > 0) {
            copy_times.push_back(copy_time);

            if (i % 100 == 0 || i == iterations - 1) {
                double bandwidth_gibps =
                    (buffer_size_mb / 1024.0) / (copy_time / 1000.0);
                std::cout << "   Iteration " << (i + 1) << "/" << iterations
                          << " → " << std::fixed << std::setprecision(2)
                          << bandwidth_gibps << " GiB/s" << std::endl;
            }
        }
    }

    BandwidthStats stats = computeBandwidthStats(copy_times, buffer_size_mb);
    if (stats.valid) {
        std::cout << std::endl;
        std::cout << "Performance Results:" << std::endl;
        std::cout << "   ├─ Average bandwidth: " << std::fixed
                  << std::setprecision(2) << stats.avgGiBps << " GiB/s"
                  << std::endl;
        std::cout << "   ├─ Min bandwidth: " << std::fixed
                  << std::setprecision(2) << stats.minGiBps << " GiB/s"
                  << std::endl;
        std::cout << "   ├─ Max bandwidth: " << std::fixed
                  << std::setprecision(2) << stats.maxGiBps << " GiB/s"
                  << std::endl;
        std::cout << "   └─ Average latency: " << std::fixed
                  << std::setprecision(3) << stats.avgLatencyMs << " ms"
                  << std::endl;
    }

    checkCudaErrorReturn(cudaFree(src_ptr), "Failed to free source memory");
    checkCudaErrorReturn(cudaFree(dst_ptr),
                         "Failed to free destination memory");
}

// Print usage information
void printUsage(const char* program_name) {
    std::cout
        << "Usage: " << program_name << " [OPTIONS]\n"
        << "Options:\n"
        << "  -i, --iterations NUM    Number of iterations (default: 100)\n"
        << "  -b, --buffer-size NUM   Buffer size in MB (default: 1000)\n"
        << "  -s, --src-gpu NUM       Source GPU ID (default: 0)\n"
        << "  -d, --dst-gpu NUM       Destination GPU ID (default: 1)\n"
        << "  -m, --mode memcpy|kernel\n"
        << "                          Copy method (default: memcpy)\n"
        << "  -h, --help              Show this help message\n"
        << "\n"
        << "Example:\n"
        << "  " << program_name << " -i 200 -b 2000 -s 0 -d 1\n"
        << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    TestConfig config = parseBwTestArgs(argc, argv);

    if (config.help) {
        printUsage(argv[0]);
        return 0;
    }
    if (!config.ok) {
        std::cerr << "Error: " << config.errorMessage << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    std::cout
        << "╔══════════════════════════════════════════════════════════════╗"
        << std::endl;
    std::cout
        << "║                    NVLink Performance Analysis               ║"
        << std::endl;
    std::cout
        << "╚══════════════════════════════════════════════════════════════╝"
        << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "   • Iterations: " << config.iterations << std::endl;
    std::cout << "   • Buffer size: " << config.buffer_size_mb << " MB"
              << std::endl;
    std::cout << "   • Source GPU: " << config.src_gpu_id << std::endl;
    std::cout << "   • Destination GPU: " << config.dst_gpu_id << std::endl;
    std::cout << "   • Copy mode: "
              << (config.mode == CopyMode::Kernel ? "kernel" : "memcpy")
              << std::endl;
    std::cout << std::endl;

    int deviceCount;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (!checkCudaErrorReturn(err, "Failed to get device count in main")) {
        return 1;
    }

    if (deviceCount < 2) {
        std::cout << "Need at least 2 GPUs for NVLink test" << std::endl;
        return 1;
    }

    // Validate GPU IDs
    if (config.src_gpu_id >= deviceCount || config.dst_gpu_id >= deviceCount) {
        std::cerr << "Error: GPU ID out of range. Available GPUs: 0-"
                  << (deviceCount - 1) << std::endl;
        return 1;
    }

    if (config.src_gpu_id == config.dst_gpu_id) {
        std::cerr << "Error: Source and destination GPU must be different"
                  << std::endl;
        return 1;
    }

    int src_gpu = config.src_gpu_id;
    int dst_gpu = config.dst_gpu_id;

    printGPUInfo(src_gpu, dst_gpu);

    if (!checkP2PSupport(src_gpu, dst_gpu)) {
        std::cout << "P2P not supported" << std::endl;
        return 1;
    }

    if (!enableP2PAccess(src_gpu, dst_gpu)) {
        std::cout << "Failed to enable P2P access" << std::endl;
        return 1;
    }

    // Run test with configured buffer size
    size_t buffer_size = config.buffer_size_mb;

    // Test copy performance (cudaMemcpyDeviceToDevice or kernel-based)
    testCopyPerformance(src_gpu, dst_gpu, buffer_size, config.iterations,
                        config.mode);

    // Test completed successfully
    std::cout << std::endl;
    std::cout << "✓ NVLink bandwidth test completed successfully!" << std::endl;

    return 0;
}