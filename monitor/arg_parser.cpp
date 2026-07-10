#include "arg_parser.h"

#include <sstream>
#include <string>

MonitorCliArgs parseMonitorArgs(int argc, char* argv[]) {
    MonitorCliArgs args;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            args.helpRequested = true;
            return args;
        } else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "-i" || arg == "--interval") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.errorMessage = "Missing value for " + arg;
                return args;
            }
            try {
                args.interval = std::stod(argv[++i]);
                if (args.interval <= 0) {
                    args.ok = false;
                    args.errorMessage = "Interval must be positive";
                    return args;
                }
            } catch (const std::exception&) {
                args.ok = false;
                args.errorMessage =
                    std::string("Invalid interval value: ") + argv[i];
                return args;
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.errorMessage = "Missing filename for " + arg;
                return args;
            }
            args.outputFilename = argv[++i];
        } else if (arg == "-f" || arg == "--format") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.errorMessage = "Missing value for " + arg;
                return args;
            }
            std::string f = argv[++i];
            if (f == "text") {
                args.format = OutputFormat::Text;
            } else if (f == "csv") {
                args.format = OutputFormat::CSV;
            } else if (f == "json") {
                args.format = OutputFormat::JSON;
            } else {
                args.ok = false;
                args.errorMessage =
                    "Invalid format: " + f + " (expected text, csv, or json)";
                return args;
            }
        } else if (arg == "-g" || arg == "--gpus") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.errorMessage = "Missing value for " + arg;
                return args;
            }
            // Parse a comma-separated list of GPU indices, e.g. "0,1,3".
            // Whitespace around tokens is tolerated.
            std::string val = argv[++i];
            std::string token;
            std::istringstream iss(val);
            while (std::getline(iss, token, ',')) {
                // Trim leading/trailing whitespace.
                size_t a = token.find_first_not_of(" \t");
                size_t b = token.find_last_not_of(" \t");
                if (a == std::string::npos) continue;  // skip empty token
                token = token.substr(a, b - a + 1);
                try {
                    int id = std::stoi(token);
                    if (id < 0) {
                        args.ok = false;
                        args.errorMessage =
                            "Invalid GPU id (negative): " + token;
                        return args;
                    }
                    args.gpuFilter.push_back(id);
                } catch (const std::exception&) {
                    args.ok = false;
                    args.errorMessage = "Invalid GPU id: " + token;
                    return args;
                }
            }
            if (args.gpuFilter.empty()) {
                args.ok = false;
                args.errorMessage = "No valid GPU ids in: " + val;
                return args;
            }
        } else if (arg == "-c" || arg == "--continuous") {
            if (i + 1 < argc && (std::string(argv[i + 1]) == "true" ||
                                 std::string(argv[i + 1]) == "false")) {
                args.continuous = (std::string(argv[++i]) == "true");
            } else {
                args.continuous = true;  // Default if no value specified
            }
        } else {
            args.ok = false;
            args.errorMessage = "Unknown option: " + arg;
            return args;
        }
    }

    return args;
}
