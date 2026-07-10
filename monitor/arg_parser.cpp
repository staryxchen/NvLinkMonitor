#include "arg_parser.h"

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
