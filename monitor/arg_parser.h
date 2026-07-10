#ifndef NVLINK_MONITOR_ARG_PARSER_H
#define NVLINK_MONITOR_ARG_PARSER_H

#include <string>

// Output format for nvlink_monitor.
//   Text - human-readable (default)
//   CSV  - machine-readable: header + one row per (gpu, link) per sample
//   JSON - JSONL: one self-contained JSON object per sample
//   (streaming-friendly)
enum class OutputFormat { Text, CSV, JSON };

// Parsed command-line arguments for nvlink_monitor.
struct MonitorCliArgs {
    double interval = 1.0;
    bool continuous = true;
    bool verbose = false;
    OutputFormat format = OutputFormat::Text;
    std::string outputFilename;
    bool helpRequested = false;
    bool ok = true;
    std::string errorMessage;
};

// Parses nvlink_monitor command-line arguments.
// On error, sets ok=false and errorMessage (does not print or exit).
MonitorCliArgs parseMonitorArgs(int argc, char* argv[]);

#endif  // NVLINK_MONITOR_ARG_PARSER_H
