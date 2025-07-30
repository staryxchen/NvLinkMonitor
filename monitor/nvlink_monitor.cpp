#include "nvlink_monitor.h"

#include <stdexcept>
#include <sched.h>

// Global flag for signal handling
volatile bool g_running = true;

// Signal handler implementation
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived stop signal, exiting..." << std::endl;
        g_running = false;
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
        result.totalTxGBps = 0.0;
        result.totalRxGBps = 0.0;

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
                linkData.rxGBps = 0.0;
                linkData.txGBps = 0.0;

                // Try Field Values API first
                nvmlFieldValue_t fieldValues[2];
                fieldValues[0].fieldId =
                    NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_TX;
                fieldValues[0].scopeId = link;
                fieldValues[1].fieldId =
                    NVML_FI_DEV_NVLINK_THROUGHPUT_DATA_RX;
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
                    // Fallback to traditional API
                    unsigned long long rxCounter, txCounter;
                    if (nvmlDeviceGetNvLinkUtilizationCounter(
                            gpu.device, link, 0, &rxCounter, &txCounter) ==
                        NVML_SUCCESS) {
                        linkData.rxBytes = rxCounter;
                        linkData.txBytes = txCounter;
                        std::cout
                            << "  Link " << link
                            << " (Traditional): TX=" << linkData.txBytes
                            << " RX=" << linkData.rxBytes << std::endl;
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
    std::vector<GPUMonitorResult> results;

    for (size_t i = 0; i < snapshot2.size(); i++) {
        if (i >= snapshot1.size()) continue;

        const auto& s1 = snapshot1[i];
        const auto& s2 = snapshot2[i];

        GPUMonitorResult result;
        result.gpuId = s2.gpuId;
        result.nvLinkCount = s2.nvLinkCount;
        result.totalTxGBps = 0.0;
        result.totalRxGBps = 0.0;

        for (size_t j = 0; j < s2.links.size(); j++) {
            if (j >= s1.links.size()) continue;

            const auto& link1 = s1.links[j];
            const auto& link2 = s2.links[j];

            // Calculate byte differences (NVML returns KiB, convert to bytes)
            long long txDelta = static_cast<long long>(link2.txBytes) -
                                static_cast<long long>(link1.txBytes);
            long long rxDelta = static_cast<long long>(link2.rxBytes) -
                                static_cast<long long>(link1.rxBytes);

            // Handle overflow cases with detailed logging
            if (txDelta < 0) {
                if (verboseOutput) {
                    std::cerr << "Warning: TX counter overflow detected on GPU " 
                              << s2.gpuId << " Link " << link2.linkId << std::endl;
                }
                txDelta = 0;
            }
            if (rxDelta < 0) {
                if (verboseOutput) {
                    std::cerr << "Warning: RX counter overflow detected on GPU " 
                              << s2.gpuId << " Link " << link2.linkId << std::endl;
                }
                rxDelta = 0;
            }

            // Convert KiB directly to GiB/s
            // NVML returns KiB, convert directly to GiB/s
            double txGiBps = static_cast<double>(txDelta) /
                             (timeDelta * 1024.0 * 1024.0);
            double rxGiBps = static_cast<double>(rxDelta) /
                             (timeDelta * 1024.0 * 1024.0);

            NvLinkData linkData;
            linkData.linkId = link2.linkId;
            linkData.txGBps = txGiBps;
            linkData.rxGBps = rxGiBps;
            linkData.txBytes = link2.txBytes;
            linkData.rxBytes = link2.rxBytes;

            result.links.push_back(linkData);
            result.totalTxGBps += txGiBps;
            result.totalRxGBps += rxGiBps;
        }

        results.push_back(result);
    }

    return results;
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
                          << std::setw(4) << gpu.totalRxGBps
                          << " GiB/s, TX: " << std::setw(4) << gpu.totalTxGBps
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
                          << std::setw(4) << gpu.totalRxGBps
                          << " GiB/s, TX: " << std::setw(4) << gpu.totalTxGBps
                          << " GiB/s" << std::endl;

        // Print individual link details
        for (const auto& link : gpu.links) {
            getOutputStream()
                << "  Link " << std::setw(2) << link.linkId
                << " RX: " << std::fixed << std::setprecision(1) << std::setw(6)
                << link.rxGBps << " GiB/s, TX: " << std::setw(6) << link.txGBps
                << " GiB/s" << std::endl;
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
        std::cout << "Set real-time scheduling priority for improved accuracy" << std::endl;
    }
    #endif

    auto lastSnapshot = getNvLinkData();
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (g_running) {
        // Use microsecond precision for sleep to improve timing accuracy
        std::this_thread::sleep_for(
            std::chrono::microseconds(static_cast<long long>(interval * 1000000)));

        if (!g_running) break;

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto currentSnapshot = getNvLinkData();

        // Calculate actual time difference with nanosecond precision
        auto timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(
            currentTime - lastTime);
        double actualInterval =
            timeDiff.count() / 1000000000.0;  // Convert to seconds with nanosecond precision

        // Check for minimum time interval to avoid division by very small numbers
        const double MIN_INTERVAL = 0.000001; // 1 microsecond minimum
        if (actualInterval < MIN_INTERVAL) {
            if (verboseOutput) {
                std::cerr << "Warning: Time interval too small (" << actualInterval 
                          << "s), using minimum interval" << std::endl;
            }
            actualInterval = MIN_INTERVAL;
        }

        auto results =
            calculateBandwidth(lastSnapshot, currentSnapshot, actualInterval);

        // Add timing precision information in verbose mode
        if (verboseOutput) {
            formatDetailedGPUResult(results);
            getOutputStream() << "  [Timing: " << std::fixed << std::setprecision(6) 
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
    // Parse command line arguments
    double interval = 1.0;
    bool continuous = true;  // Default to continuous mode
    bool verbose = false;
    std::string outputFilename = "";  // Output file name

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Help options
        if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        }
        // Verbose options
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
        // Interval options
        else if (arg == "-i" || arg == "--interval") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Missing value for " << arg << std::endl;
                printHelp(argv[0]);
                return 1;
            }
            try {
                interval = std::stod(argv[++i]);
                if (interval <= 0) {
                    std::cerr << "Error: Interval must be positive"
                              << std::endl;
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid interval value: " << argv[i]
                          << std::endl;
                return 1;
            }
        }
        // Output file options
        else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Missing filename for " << arg << std::endl;
                printHelp(argv[0]);
                return 1;
            }
            outputFilename = argv[++i];
        }
        // Continuous options
        else if (arg == "-c" || arg == "--continuous") {
            if (i + 1 < argc && (std::string(argv[i + 1]) == "true" ||
                                 std::string(argv[i + 1]) == "false")) {
                continuous = (std::string(argv[++i]) == "true");
            } else {
                continuous = true;  // Default to true if no value specified
            }
        }
        // Unknown option
        else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printHelp(argv[0]);
            return 1;
        }
    }

    // Setup signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        NvLinkMonitor monitor(verbose, outputFilename);

        if (continuous) {
            monitor.runContinuousMonitoring(interval);
        } else {
            monitor.runSingleMonitoring(interval);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}