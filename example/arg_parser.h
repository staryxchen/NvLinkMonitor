#ifndef NVLINK_BW_TEST_ARG_PARSER_H
#define NVLINK_BW_TEST_ARG_PARSER_H

#include <cstddef>
#include <string>

// How data is moved between GPUs during the bandwidth test.
//   Memcpy - cudaMemcpyDeviceToDevice (driver-managed DMA via copy engine)
//   Kernel - a __global__ copy kernel issuing vectorized load/stores over P2P
enum class CopyMode { Memcpy, Kernel };

// Direction of the copy test.
//   Unidir - src -> dst only (one-way bandwidth)
//   Bidir  - src -> dst AND dst -> src concurrently; reports aggregate
//            full-duplex bandwidth (2 * size / elapsed)
enum class Direction { Unidir, Bidir };

// Parsed command-line arguments for nvlink_bw_test.
struct TestConfig {
    int iterations = 100;
    size_t buffer_size_mb = 1000;
    int src_gpu_id = 0;
    int dst_gpu_id = 1;
    CopyMode mode = CopyMode::Memcpy;
    Direction direction = Direction::Unidir;
    bool allPairs = false;  // --all-pairs: sweep all i<j GPU pairs
    bool help = false;
    bool ok = true;
    std::string errorMessage;
};

// Parses nvlink_bw_test command-line arguments.
// On error, sets ok=false and errorMessage (does not print or exit).
TestConfig parseBwTestArgs(int argc, char* argv[]);

#endif  // NVLINK_BW_TEST_ARG_PARSER_H
