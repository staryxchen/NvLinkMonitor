#include "arg_parser.h"

#include <getopt.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <string>

namespace {

// Parses a base-10 integer from `s` into `out`. Returns false and sets `err`
// on parse failure (non-numeric trailing chars) or ERANGE overflow.
bool parseLong(const char* s, long& out, std::string& err, const char* name) {
    char* end = nullptr;
    errno = 0;
    out = std::strtol(s, &end, 10);
    if (errno == ERANGE) {
        err = std::string(name) + " out of range";
        return false;
    }
    if (end == s || *end != '\0') {
        err = std::string("invalid ") + name + ": " + s;
        return false;
    }
    return true;
}

}  // namespace

TestConfig parseBwTestArgs(int argc, char* argv[]) {
    TestConfig config;

    static struct option long_options[] = {
        {"iterations", required_argument, 0, 'i'},
        {"buffer-size", required_argument, 0, 'b'},
        {"src-gpu", required_argument, 0, 's'},
        {"dst-gpu", required_argument, 0, 'd'},
        {"mode", required_argument, 0, 'm'},
        {"direction", required_argument, 0, 1001},
        {"all-pairs", no_argument, 0, 1002},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    // Reset getopt's global state so repeated calls (e.g. in tests) re-parse
    // from the beginning. glibc resets internal state when optind is set to 0.
    optind = 0;

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "i:b:s:d:m:h", long_options,
                              &option_index)) != -1) {
        switch (opt) {
            case 'i': {
                long val;
                if (!parseLong(optarg, val, config.errorMessage,
                               "iterations")) {
                    config.ok = false;
                    return config;
                }
                if (val <= 0) {
                    config.ok = false;
                    config.errorMessage = "iterations must be positive";
                    return config;
                }
                config.iterations = static_cast<int>(val);
                break;
            }
            case 'b': {
                long val;
                if (!parseLong(optarg, val, config.errorMessage,
                               "buffer size")) {
                    config.ok = false;
                    return config;
                }
                if (val <= 0) {
                    config.ok = false;
                    config.errorMessage = "buffer size must be positive";
                    return config;
                }
                config.buffer_size_mb = static_cast<size_t>(val);
                break;
            }
            case 's': {
                long val;
                if (!parseLong(optarg, val, config.errorMessage,
                               "source GPU ID")) {
                    config.ok = false;
                    return config;
                }
                if (val < 0) {
                    config.ok = false;
                    config.errorMessage = "source GPU ID must be non-negative";
                    return config;
                }
                config.src_gpu_id = static_cast<int>(val);
                break;
            }
            case 'd': {
                long val;
                if (!parseLong(optarg, val, config.errorMessage,
                               "destination GPU ID")) {
                    config.ok = false;
                    return config;
                }
                if (val < 0) {
                    config.ok = false;
                    config.errorMessage =
                        "destination GPU ID must be non-negative";
                    return config;
                }
                config.dst_gpu_id = static_cast<int>(val);
                break;
            }
            case 'm': {
                std::string m = optarg;
                if (m == "memcpy") {
                    config.mode = CopyMode::Memcpy;
                } else if (m == "kernel") {
                    config.mode = CopyMode::Kernel;
                } else {
                    config.ok = false;
                    config.errorMessage = "invalid mode: " + m +
                                          " (expected 'memcpy' or 'kernel')";
                    return config;
                }
                break;
            }
            case 1001: {  // --direction
                std::string d = optarg;
                if (d == "unidir") {
                    config.direction = Direction::Unidir;
                } else if (d == "bidir") {
                    config.direction = Direction::Bidir;
                } else {
                    config.ok = false;
                    config.errorMessage = "invalid direction: " + d +
                                          " (expected 'unidir' or 'bidir')";
                    return config;
                }
                break;
            }
            case 1002:  // --all-pairs
                config.allPairs = true;
                break;
            case 'h':
                config.help = true;
                break;
            case '?':
                // getopt already printed an error message to stderr.
                config.ok = false;
                config.errorMessage = "invalid command-line arguments";
                return config;
            default:
                config.ok = false;
                config.errorMessage = "unexpected option";
                return config;
        }
    }

    return config;
}
