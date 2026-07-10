#ifndef NVLINK_MONITOR_ARG_PARSER_H
#define NVLINK_MONITOR_ARG_PARSER_H

#include <string>

// Parsed command-line arguments for nvlink_monitor.
struct MonitorCliArgs {
    double interval = 1.0;
    bool continuous = true;
    bool verbose = false;
    std::string outputFilename;
    bool helpRequested = false;
    bool ok = true;
    std::string errorMessage;
};

// Parses nvlink_monitor command-line arguments.
// On error, sets ok=false and errorMessage (does not print or exit).
MonitorCliArgs parseMonitorArgs(int argc, char* argv[]);

#endif  // NVLINK_MONITOR_ARG_PARSER_H
