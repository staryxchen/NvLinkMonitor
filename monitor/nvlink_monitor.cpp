#include "nvlink_monitor.h"

#include <sched.h>

#include <stdexcept>

#include "arg_parser.h"
#include "bandwidth_calc.h"

// Global flag for signal handling. sig_atomic_t guarantees that writes from
// the signal handler are well-defined. The handler only flips this flag — it
// must NOT do any I/O (std::cout/cerr are not async-signal-safe and can
// deadlock if the signal interrupts the main thread mid-output).
volatile sig_atomic_t g_running = 1;

// Signal handler implementation — async-signal-safe: only flips g_running.
// The "exiting" notice is printed by the main loop after it observes the flag.
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = 0;
    }
}

// NvLinkMonitor constructor
NvLinkMonitor::NvLinkMonitor(bool verbose, const std::string& outputFilename)
    : verboseOutput(verbose), fileOutput(!outputFilename.empty()) {
    // Initialize NVML
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        std::cerr << "Failed to initialize NVML: " << nvmlErrorString(result)
                  << std::endl;
        throw std::runtime_error("NVML initialization failed");
    }

    // Open output file if specified
    if (fileOutput) {
        outputFile.open(outputFilename, std::ios::out | std::ios::app);
        if (!outputFile.is_open()) {
            std::cerr << "Failed to open output file: " << outputFilename
                      << std::endl;
            throw std::runtime_error("Failed to open output file");
        }
    }

    // Discover GPUs
    discoverGPUs();
}

// NvLinkMonitor destructor
NvLinkMonitor::~NvLinkMonitor() {
    if (fileOutput && outputFile.is_open()) {
        outputFile.close();
    }
    nvmlShutdown();
}

void NvLinkMonitor::discoverGPUs() {
    unsigned int deviceCount = 0;
    nvmlReturn_t result = nvmlDeviceGetCount(&deviceCount);
    if (result != NVML_SUCCESS) {
        std::cerr << "Failed to get device count: " << nvmlErrorString(result)
                  << std::endl;
        return;
    }

    std::cout << "Found " << deviceCount << " GPU(s)" << std::endl;

    for (unsigned int i = 0; i < deviceCount; i++) {
        nvmlDevice_t device;
        result = nvmlDeviceGetHandleByIndex(i, &device);
        if (result != NVML_SUCCESS) {
            std::cerr << "Failed to get device handle for GPU " << i << ": "
                      << nvmlErrorString(result) << std::endl;
            continue;
        }

        // Get device name
        char name[NVML_DEVICE_NAME_BUFFER_SIZE];
        result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
        if (result != NVML_SUCCESS) {
            std::cerr << "Failed to get device name for GPU " << i << ": "
                      << nvmlErrorString(result) << std::endl;
        }

        // Get device UUID
        char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
        result = nvmlDeviceGetUUID(device, uuid, NVML_DEVICE_UUID_BUFFER_SIZE);
        if (result != NVML_SUCCESS) {
            std::cerr << "Failed to get device UUID for GPU " << i << ": "
                      << nvmlErrorString(result) << std::endl;
        }

        // Get NvLink count for this GPU
        unsigned int nvLinkCount = 0;
        nvmlFieldValue_t fieldValues[1];
        fieldValues[0].fieldId = NVML_FI_DEV_NVLINK_LINK_COUNT;
        fieldValues[0].scopeId = 0;
        nvmlReturn_t fieldResult =
            nvmlDeviceGetFieldValues(device, 1, fieldValues);
        if (fieldResult == NVML_SUCCESS) {
            nvLinkCount = fieldValues[0].value.uiVal;
        }

        GPUData gpu;
        gpu.id = std::to_string(i);
        gpu.uuid = std::string(uuid);
        gpu.device = device;
        gpu.nvLinkCount = nvLinkCount;

        gpus.push_back(gpu);
        std::cout << "GPU " << i << ": " << name << " (UUID: " << uuid << ") - "
                  << nvLinkCount << " NvLinks" << std::endl;
    }
}

std::vector<GPUMonitorResult> NvLinkMonitor::getNvLinkData() {
    std::vector<GPUMonitorResult> results;

    for (const auto& gpu : gpus) {
        GPUMonitorResult result;
        result.gpuId = gpu.id;
        result.nvLinkCount = gpu.nvLinkCount;
        result.totalTxGiBps = 0.0;
        result.totalRxGiBps = 0.0;

        // Get utilization counters for each NvLink using Field Values
        for (unsigned int link = 0; link < gpu.nvLinkCount; link++) {
            nvmlEnableState_t isActive;
            if (nvmlDeviceGetNvLinkState(gpu.device, link, &isActive) ==
                    NVML_SUCCESS &&
                isActive == NVML_FEATURE_ENABLED) {
                NvLinkData linkData;
                linkData.linkId = link;
                linkData.rxBytes = 0;
                linkData.txBytes = 0;
                linkData.rxGiBps = 0.0;
                linkData.txGiBps = 0.0;

                // Try Field Values API first
                nvmlFieldValue_t fieldValues[2];
                fieldValues[0].fieldId = NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX;
                fieldValues[0].scopeId = link;
                fieldValues[1].fieldId = NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_RX;
                fieldValues[1].scopeId = link;

                nvmlReturn_t fieldResult =
                    nvmlDeviceGetFieldValues(gpu.device, 2, fieldValues);
                if (fieldResult == NVML_SUCCESS) {
                    // Extract TX counter
                    if (fieldValues[0].nvmlReturn == NVML_SUCCESS &&
                        fieldValues[0].valueType ==
                            NVML_VALUE_TYPE_UNSIGNED_LONG_LONG) {
                        linkData.txBytes = fieldValues[0].value.ullVal;
                    }

                    // Extract RX counter
                    if (fieldValues[1].nvmlReturn == NVML_SUCCESS &&
                        fieldValues[1].valueType ==
                            NVML_VALUE_TYPE_UNSIGNED_LONG_LONG) {
                        linkData.rxBytes = fieldValues[1].value.ullVal;
                    }
                } else {
                    // Fallback to the traditional utilization-counter API.
                    // CAVEAT: unlike NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX/RX
                    // (which are documented as KiB throughput), the raw
                    // counters from nvmlDeviceGetNvLinkUtilizationCounter have
                    // units that depend on the counter configuration set via
                    // nvmlDeviceSetNvLinkUtilizationCounter, and are not
                    // guaranteed to be KiB. We feed them through the same
                    // KiB->GiB conversion in bandwidth_calc as a best-effort
                    // estimate, so bandwidth numbers from this path may be
                    // inaccurate. Warn once per link in verbose mode.
                    unsigned long long rxCounter, txCounter;
                    if (nvmlDeviceGetNvLinkUtilizationCounter(
                            gpu.device, link, 0, &rxCounter, &txCounter) ==
                        NVML_SUCCESS) {
                        linkData.rxBytes = rxCounter;
                        linkData.txBytes = txCounter;
                        if (verboseOutput) {
                            std::cerr
                                << "Warning: GPU " << gpu.id << " Link " << link
                                << " using traditional utilization counter "
                                << "(units may differ from KiB throughput; "
                                << "bandwidth estimate may be inaccurate)"
                                << std::endl;
                        }
                    } else {
                        std::cerr
                            << "Failed to get utilization counters for GPU "
                            << gpu.id << " Link " << link << ": "
                            << nvmlErrorString(fieldResult) << std::endl;
                    }
                }

                result.links.push_back(linkData);
            }
        }

        results.push_back(result);
    }

    return results;
}

std::ostream& NvLinkMonitor::getOutputStream() {
    return fileOutput ? outputFile : std::cout;
}

std::vector<GPUMonitorResult> NvLinkMonitor::calculateBandwidth(
    const std::vector<GPUMonitorResult>& snapshot1,
    const std::vector<GPUMonitorResult>& snapshot2, double timeDelta) {
    return ::calculateBandwidth(snapshot1, snapshot2, timeDelta, verboseOutput);
}

void NvLinkMonitor::formatGPUResult(
    const std::vector<GPUMonitorResult>& results) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    // Print header with timestamp
    getOutputStream() << "+--- NvLink Monitor --- "
                      << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ---+"
                      << std::endl;

    // Print GPU results
    for (const auto& gpu : results) {
        getOutputStream() << "GPU " << gpu.gpuId << " (" << gpu.nvLinkCount
                          << " links) "
                          << "RX: " << std::fixed << std::setprecision(1)
                          << std::setw(4) << gpu.totalRxGiBps
                          << " GiB/s, TX: " << std::setw(4) << gpu.totalTxGiBps
                          << " GiB/s" << std::endl;
    }
}

void NvLinkMonitor::formatDetailedGPUResult(
    const std::vector<GPUMonitorResult>& results) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    // Print header with timestamp
    getOutputStream() << "+--- NvLink Monitor (Detailed) --- "
                      << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ---+"
                      << std::endl;

    // Print detailed GPU results
    for (const auto& gpu : results) {
        getOutputStream() << "GPU " << gpu.gpuId << " (" << gpu.nvLinkCount
                          << " links) "
                          << "Total RX: " << std::fixed << std::setprecision(1)
                          << std::setw(4) << gpu.totalRxGiBps
                          << " GiB/s, TX: " << std::setw(4) << gpu.totalTxGiBps
                          << " GiB/s" << std::endl;

        // Print individual link details
        for (const auto& link : gpu.links) {
            getOutputStream()
                << "  Link " << std::setw(2) << link.linkId
                << " RX: " << std::fixed << std::setprecision(1) << std::setw(6)
                << link.rxGiBps << " GiB/s, TX: " << std::setw(6)
                << link.txGiBps << " GiB/s" << std::endl;
        }
        getOutputStream() << std::endl;  // Add blank line between GPUs
    }
}

void NvLinkMonitor::runContinuousMonitoring(double interval) {
    std::cout << "Starting continuous monitoring, interval: " << interval << "s"
              << std::endl;
    std::cout << "Press Ctrl+C to stop monitoring" << std::endl;

// Set high priority for more accurate timing
#ifdef _GNU_SOURCE
    // Try to set real-time priority for better timing accuracy
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == 0) {
        std::cout << "Set real-time scheduling priority for improved accuracy"
                  << std::endl;
    }
#endif

    auto lastSnapshot = getNvLinkData();
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (g_running) {
        // Use microsecond precision for sleep to improve timing accuracy
        std::this_thread::sleep_for(std::chrono::microseconds(
            static_cast<long long>(interval * 1000000)));

        if (!g_running) break;

        // Record the timestamp AFTER reading the counters so that both
        // lastTime and currentTime mark the moment a counter read completed.
        // actualInterval then exactly equals the observation window between
        // two reads (which includes the previous iteration's calculate/print
        // time — NVML counters keep accumulating during that work, so it
        // belongs in the denominator). Recording the timestamp before the
        // read would make iteration 1 exclude the read duration while
        // iteration 2+ include it, producing inconsistent deltas.
        auto currentSnapshot = getNvLinkData();
        auto currentTime = std::chrono::high_resolution_clock::now();

        // Calculate actual time difference with nanosecond precision
        auto timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(
            currentTime - lastTime);
        double actualInterval =
            timeDiff.count() /
            1000000000.0;  // Convert to seconds with nanosecond precision

        // Check for minimum time interval to avoid division by very small
        // numbers
        const double MIN_INTERVAL = 0.000001;  // 1 microsecond minimum
        if (actualInterval < MIN_INTERVAL) {
            if (verboseOutput) {
                std::cerr << "Warning: Time interval too small ("
                          << actualInterval << "s), using minimum interval"
                          << std::endl;
            }
            actualInterval = MIN_INTERVAL;
        }

        auto results =
            calculateBandwidth(lastSnapshot, currentSnapshot, actualInterval);

        // Add timing precision information in verbose mode
        if (verboseOutput) {
            formatDetailedGPUResult(results);
            getOutputStream()
                << "  [Timing: " << std::fixed << std::setprecision(6)
                << actualInterval << "s]" << std::endl;
        } else {
            formatGPUResult(results);
        }

        lastSnapshot = currentSnapshot;
        lastTime = currentTime;
    }
}

void NvLinkMonitor::runSingleMonitoring(double interval) {
    std::cout << "Getting first snapshot..." << std::endl;
    auto snapshot1 = getNvLinkData();

    std::cout << "Waiting " << interval << "s to get second snapshot..."
              << std::endl;
    // Use microsecond precision for sleep to improve timing accuracy
    std::this_thread::sleep_for(
        std::chrono::microseconds(static_cast<long long>(interval * 1000000)));

    auto snapshot2 = getNvLinkData();
    auto results = calculateBandwidth(snapshot1, snapshot2, interval);

    if (verboseOutput) {
        formatDetailedGPUResult(results);
    } else {
        formatGPUResult(results);
    }
}

void printHelp(const char* programName) {
    std::cout << "NvLink Monitor Usage:" << std::endl;
    std::cout << "  " << programName << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --continuous [true|false] : Run in continuous mode "
                 "(default: true)"
              << std::endl;
    std::cout << "  -i, --interval <seconds>      : Set monitoring interval in "
                 "seconds (default: 1.0)"
              << std::endl;
    std::cout
        << "  -v, --verbose                 : Enable detailed NvLink output"
        << std::endl;
    std::cout << "  -o, --output <filename>       : Redirect output to file"
              << std::endl;
    std::cout << "  -h, --help                    : Show this help message"
              << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName
              << "                           # Continuous mode (default)"
              << std::endl;
    std::cout << "  " << programName
              << " --continuous true         # Continuous mode" << std::endl;
    std::cout << "  " << programName
              << " -c false                  # Single monitoring mode"
              << std::endl;
    std::cout << "  " << programName
              << " --verbose                 # Detailed output" << std::endl;
    std::cout << "  " << programName
              << " -i 2.0 -v                # Custom interval with verbose"
              << std::endl;
    std::cout << "  " << programName
              << " -c false -i 2.0 --verbose # Combined options" << std::endl;
    std::cout << "  " << programName
              << " -o output.log             # Output to file" << std::endl;
    std::cout << "  " << programName
              << " -v -o detailed.log        # Verbose output to file"
              << std::endl;
}

int main(int argc, char* argv[]) {
    MonitorCliArgs args = parseMonitorArgs(argc, argv);

    if (args.helpRequested) {
        printHelp(argv[0]);
        return 0;
    }
    if (!args.ok) {
        std::cerr << "Error: " << args.errorMessage << std::endl;
        printHelp(argv[0]);
        return 1;
    }

    // Setup signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        NvLinkMonitor monitor(args.verbose, args.outputFilename);

        if (args.continuous) {
            monitor.runContinuousMonitoring(args.interval);
        } else {
            monitor.runSingleMonitoring(args.interval);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // Printed from the main thread (not the signal handler) because std::cout
    // is not async-signal-safe. The handler only flips g_running; this covers
    // both continuous and single monitoring modes uniformly.
    if (!g_running) {
        std::cout << "\nReceived stop signal, exiting..." << std::endl;
    }

    return 0;
}