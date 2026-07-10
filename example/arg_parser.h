#ifndef NVLINK_BW_TEST_ARG_PARSER_H
#define NVLINK_BW_TEST_ARG_PARSER_H

#include <cstddef>
#include <string>

// Parsed command-line arguments for nvlink_bw_test.
struct TestConfig {
    int iterations = 100;
    size_t buffer_size_mb = 1000;
    int src_gpu_id = 0;
    int dst_gpu_id = 1;
    bool help = false;
    bool ok = true;
    std::string errorMessage;
};

// Parses nvlink_bw_test command-line arguments.
// On error, sets ok=false and errorMessage (does not print or exit).
TestConfig parseBwTestArgs(int argc, char* argv[]);

#endif  // NVLINK_BW_TEST_ARG_PARSER_H
