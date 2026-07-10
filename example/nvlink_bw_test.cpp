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

// Times a bidirectional copy: src->dst on streamA (src_gpu) and dst->src on
// streamB (dst_gpu), issued concurrently so both directions share the
// wall-clock window. Returns max(elapsedA, elapsedB) in ms (the time for the
// slower direction to finish), or -1.0 on error. The caller computes
// aggregate full-duplex bandwidth as 2 * size / elapsed.
double measureBidirCopyTime(void* src_ptr, void* dst_ptr, size_t size_bytes,
                            int src_gpu, int dst_gpu, CopyMode mode) {
    cudaStream_t streamA = nullptr, streamB = nullptr;
    cudaEvent_t startA = nullptr, stopA = nullptr;
    cudaEvent_t startB = nullptr, stopB = nullptr;

    auto cleanup = [&]() {
        if (streamA) cudaStreamDestroy(streamA);
        if (streamB) cudaStreamDestroy(streamB);
        if (startA) cudaEventDestroy(startA);
        if (stopA) cudaEventDestroy(stopA);
        if (startB) cudaEventDestroy(startB);
        if (stopB) cudaEventDestroy(stopB);
    };

    // Per-GPU streams so the two copies run concurrently (separate copy
    // engines / SMs). Each stream AND its events are created on the owning
    // device — a cudaEvent_t recorded into a stream on a different device
    // yields cudaErrorInvalidResourceHandle.
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set src device for bidir") ||
        !checkCudaErrorReturn(cudaStreamCreate(&streamA),
                              "Failed to create src stream") ||
        !checkCudaErrorReturn(cudaEventCreate(&startA),
                              "Failed to create startA") ||
        !checkCudaErrorReturn(cudaEventCreate(&stopA),
                              "Failed to create stopA") ||
        !checkCudaErrorReturn(cudaSetDevice(dst_gpu),
                              "Failed to set dst device for bidir") ||
        !checkCudaErrorReturn(cudaStreamCreate(&streamB),
                              "Failed to create dst stream") ||
        !checkCudaErrorReturn(cudaEventCreate(&startB),
                              "Failed to create startB") ||
        !checkCudaErrorReturn(cudaEventCreate(&stopB),
                              "Failed to create stopB")) {
        cleanup();
        return -1.0;
    }

    // Sync both devices so timing starts from a quiet point, then record both
    // starts on their streams (the two starts are ~simultaneous).
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set src device for sync") ||
        !checkCudaErrorReturn(cudaDeviceSynchronize(),
                              "Failed to sync src device") ||
        !checkCudaErrorReturn(cudaSetDevice(dst_gpu),
                              "Failed to set dst device for sync") ||
        !checkCudaErrorReturn(cudaDeviceSynchronize(),
                              "Failed to sync dst device") ||
        !checkCudaErrorReturn(cudaEventRecord(startA, streamA),
                              "Failed to record startA") ||
        !checkCudaErrorReturn(cudaEventRecord(startB, streamB),
                              "Failed to record startB")) {
        cleanup();
        return -1.0;
    }

    // Direction A: src->dst (launched on src_gpu / streamA).
    // Direction B: dst->src (launched on dst_gpu / streamB).
    if (mode == CopyMode::Kernel) {
        // launchCopyKernel sets the device itself and uses the given stream.
        cudaError_t errA =
            launchCopyKernel(dst_ptr, src_ptr, size_bytes, src_gpu, streamA);
        cudaError_t errB =
            launchCopyKernel(src_ptr, dst_ptr, size_bytes, dst_gpu, streamB);
        if (errA != cudaSuccess || errB != cudaSuccess) {
            std::cerr << "Bidir kernel launch failed (A=" << errA
                      << " B=" << errB << ")" << std::endl;
            cleanup();
            return -1.0;
        }
    } else {
        if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                                  "Failed to set src device for memcpy A") ||
            !checkCudaErrorReturn(
                cudaMemcpyAsync(dst_ptr, src_ptr, size_bytes,
                                cudaMemcpyDeviceToDevice, streamA),
                "Bidir memcpy A failed") ||
            !checkCudaErrorReturn(cudaSetDevice(dst_gpu),
                                  "Failed to set dst device for memcpy B") ||
            !checkCudaErrorReturn(
                cudaMemcpyAsync(src_ptr, dst_ptr, size_bytes,
                                cudaMemcpyDeviceToDevice, streamB),
                "Bidir memcpy B failed")) {
            cleanup();
            return -1.0;
        }
    }

    // Record both stops and sync both. Execution errors surface at the syncs.
    if (!checkCudaErrorReturn(cudaEventRecord(stopA, streamA),
                              "Failed to record stopA") ||
        !checkCudaErrorReturn(cudaEventRecord(stopB, streamB),
                              "Failed to record stopB") ||
        !checkCudaErrorReturn(cudaEventSynchronize(stopA),
                              "Bidir copy A failed") ||
        !checkCudaErrorReturn(cudaEventSynchronize(stopB),
                              "Bidir copy B failed")) {
        cleanup();
        return -1.0;
    }

    float msA = 0.0f, msB = 0.0f;
    if (!checkCudaErrorReturn(cudaEventElapsedTime(&msA, startA, stopA),
                              "Failed to get elapsed A") ||
        !checkCudaErrorReturn(cudaEventElapsedTime(&msB, startB, stopB),
                              "Failed to get elapsed B")) {
        cleanup();
        return -1.0;
    }

    cleanup();
    // Wall-clock for both directions to finish = the slower one.
    return static_cast<double>(std::max(msA, msB));
}

BandwidthStats testCopyPerformance(int src_gpu, int dst_gpu,
                                   size_t buffer_size_mb, int iterations,
                                   CopyMode mode, Direction direction) {
    bool bidir = (direction == Direction::Bidir);
    std::cout << "Buffer size: " << buffer_size_mb
              << " MB | Iterations: " << iterations << " | Direction: "
              << (bidir ? "bidirectional (aggregate)" : "unidirectional")
              << std::endl;

    void *src_ptr, *dst_ptr;
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set source device for allocation")) {
        return BandwidthStats{};
    }

    if (!checkCudaErrorReturn(
            cudaMalloc(&src_ptr, buffer_size_mb * 1024 * 1024),
            "Failed to allocate source memory")) {
        return BandwidthStats{};
    }

    if (!checkCudaErrorReturn(
            cudaSetDevice(dst_gpu),
            "Failed to set destination device for allocation")) {
        cudaFree(src_ptr);
        return BandwidthStats{};
    }

    if (!checkCudaErrorReturn(
            cudaMalloc(&dst_ptr, buffer_size_mb * 1024 * 1024),
            "Failed to allocate destination memory")) {
        cudaFree(src_ptr);
        return BandwidthStats{};
    }

    // Initialize source buffer
    if (!checkCudaErrorReturn(cudaSetDevice(src_gpu),
                              "Failed to set source device for memset")) {
        cudaFree(src_ptr);
        cudaFree(dst_ptr);
        return BandwidthStats{};
    }

    if (!checkCudaErrorReturn(
            cudaMemset(src_ptr, 0x42, buffer_size_mb * 1024 * 1024),
            "Failed to initialize source buffer")) {
        cudaFree(src_ptr);
        cudaFree(dst_ptr);
        return BandwidthStats{};
    }

    // In bidirectional mode dst->src also copies, so initialize dst too (not
    // strictly required for a bandwidth measurement, but avoids copying
    // uninitialized memory).
    if (bidir) {
        if (!checkCudaErrorReturn(cudaSetDevice(dst_gpu),
                                  "Failed to set dst device for memset") ||
            !checkCudaErrorReturn(
                cudaMemset(dst_ptr, 0x24, buffer_size_mb * 1024 * 1024),
                "Failed to initialize dst buffer")) {
            cudaFree(src_ptr);
            cudaFree(dst_ptr);
            return BandwidthStats{};
        }
    }

    if (!checkCudaErrorReturn(cudaDeviceSynchronize(),
                              "Failed to synchronize device after memset")) {
        cudaFree(src_ptr);
        cudaFree(dst_ptr);
        return BandwidthStats{};
    }

    // Warmup: one untimed copy matching the selected mode/direction to prime
    // the CUDA context, copy engine / kernel launch path, and event/stream
    // infrastructure, so the first timed iteration is not skewed by one-time
    // setup costs. Abort if the warmup itself fails.
    size_t size_bytes = buffer_size_mb * 1024 * 1024;
    double warmup_time;
    if (bidir) {
        warmup_time = measureBidirCopyTime(src_ptr, dst_ptr, size_bytes,
                                           src_gpu, dst_gpu, mode);
    } else if (mode == CopyMode::Kernel) {
        warmup_time =
            measureCopyTimeKernel(dst_ptr, src_ptr, size_bytes, src_gpu);
    } else {
        warmup_time = measureCopyTime(dst_ptr, src_ptr, size_bytes, src_gpu,
                                      cudaMemcpyDeviceToDevice);
    }
    if (warmup_time <= 0) {
        std::cerr << "Warmup copy failed; aborting test" << std::endl;
        checkCudaErrorReturn(cudaFree(src_ptr), "Failed to free source memory");
        checkCudaErrorReturn(cudaFree(dst_ptr),
                             "Failed to free destination memory");
        return BandwidthStats{};
    }

    std::vector<double> copy_times;
    // Per-iteration reported bandwidth: 2x in bidir (two buffers move).
    double bwScale = bidir ? 2.0 : 1.0;

    for (int i = 0; i < iterations; i++) {
        double copy_time;
        if (bidir) {
            copy_time = measureBidirCopyTime(src_ptr, dst_ptr, size_bytes,
                                             src_gpu, dst_gpu, mode);
        } else if (mode == CopyMode::Kernel) {
            copy_time =
                measureCopyTimeKernel(dst_ptr, src_ptr, size_bytes, src_gpu);
        } else {
            copy_time = measureCopyTime(dst_ptr, src_ptr, size_bytes, src_gpu,
                                        cudaMemcpyDeviceToDevice);
        }

        if (copy_time > 0) {
            copy_times.push_back(copy_time);

            if (i % 100 == 0 || i == iterations - 1) {
                double bandwidth_gibps =
                    bwScale * (buffer_size_mb / 1024.0) / (copy_time / 1000.0);
                std::cout << "   Iteration " << (i + 1) << "/" << iterations
                          << " → " << std::fixed << std::setprecision(2)
                          << bandwidth_gibps << " GiB/s" << std::endl;
            }
        }
    }

    BandwidthStats stats =
        computeBandwidthStats(copy_times, buffer_size_mb, bidir);
    if (stats.valid) {
        std::cout << std::endl;
        std::cout << "Performance Results"
                  << (bidir ? " (aggregate, both directions)" : "") << ":"
                  << std::endl;
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
    return stats;
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
        << "      --direction unidir|bidir\n"
        << "                          unidir (default) or bidir (concurrent\n"
        << "                          both-direction, reports aggregate BW)\n"
        << "      --all-pairs         Sweep all i<j GPU pairs (ignores -s/-d)\n"
        << "  -h, --help              Show this help message\n"
        << "\n"
        << "Examples:\n"
        << "  " << program_name << " -i 200 -b 2000 -s 0 -d 1\n"
        << "  " << program_name
        << " --direction bidir -s 0 -d 1   # full-duplex aggregate BW\n"
        << "  " << program_name
        << " --all-pairs -i 20 -b 500      # topology sweep over all pairs\n"
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
    if (config.allPairs) {
        std::cout << "   • Mode: all-pairs sweep" << std::endl;
    } else {
        std::cout << "   • Source GPU: " << config.src_gpu_id << std::endl;
        std::cout << "   • Destination GPU: " << config.dst_gpu_id << std::endl;
    }
    std::cout << "   • Copy method: "
              << (config.mode == CopyMode::Kernel ? "kernel" : "memcpy")
              << std::endl;
    std::cout << "   • Direction: "
              << (config.direction == Direction::Bidir ? "bidir (aggregate)"
                                                       : "unidir")
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

    size_t buffer_size = config.buffer_size_mb;

    if (config.allPairs) {
        // Sweep all i<j GPU pairs for topology validation. -s/-d are ignored.
        int numPairs = deviceCount * (deviceCount - 1) / 2;
        std::cout << "Sweeping all GPU pairs (i<j): " << deviceCount
                  << " GPUs, " << numPairs << " pairs" << std::endl;
        std::cout << std::endl;

        struct PairResult {
            int a, b;
            double avgGiBps;
            bool valid;
        };
        std::vector<PairResult> results;

        for (int i = 0; i < deviceCount; i++) {
            for (int j = i + 1; j < deviceCount; j++) {
                std::cout << "═══ Pair GPU " << i << " ↔ GPU " << j << " ═══"
                          << std::endl;
                if (!checkP2PSupport(i, j)) {
                    std::cout << "  P2P not supported, skipping" << std::endl
                              << std::endl;
                    results.push_back({i, j, 0.0, false});
                    continue;
                }
                if (!enableP2PAccess(i, j)) {
                    std::cout << "  Failed to enable P2P, skipping" << std::endl
                              << std::endl;
                    results.push_back({i, j, 0.0, false});
                    continue;
                }
                BandwidthStats s =
                    testCopyPerformance(i, j, buffer_size, config.iterations,
                                        config.mode, config.direction);
                results.push_back({i, j, s.avgGiBps, s.valid});
                std::cout << std::endl;
            }
        }

        // Compact summary.
        std::cout << "═══════════ All-Pairs Summary ═══════════" << std::endl;
        for (const auto& r : results) {
            std::cout << "  GPU " << r.a << " ↔ GPU " << r.b << ": ";
            if (r.valid) {
                std::cout << std::fixed << std::setprecision(2) << r.avgGiBps
                          << " GiB/s avg" << std::endl;
            } else {
                std::cout << "n/a (no P2P or failed)" << std::endl;
            }
        }
        std::cout << std::endl;
        std::cout << "✓ NVLink all-pairs sweep completed!" << std::endl;
        return 0;
    }

    // Single-pair mode.
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

    // Test copy performance (cudaMemcpyDeviceToDevice or kernel-based;
    // unidirectional or bidirectional).
    testCopyPerformance(src_gpu, dst_gpu, buffer_size, config.iterations,
                        config.mode, config.direction);

    // Test completed successfully
    std::cout << std::endl;
    std::cout << "✓ NVLink bandwidth test completed successfully!" << std::endl;

    return 0;
}