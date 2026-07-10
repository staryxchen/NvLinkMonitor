#ifndef NVLINK_MONITOR_H
#define NVLINK_MONITOR_H

#include <nvml.h>
#include <signal.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Global flag for signal handling
extern volatile bool g_running;

// Signal handler declaration
void signal_handler(int signal);

// Structure to hold GPU data
struct GPUData {
    std::string id;
    std::string uuid;
    nvmlDevice_t device;       // Single device handle
    unsigned int nvLinkCount;  // Number of active NvLinks
};

// Structure to hold NvLink utilization data
struct NvLinkData {
    unsigned int linkId;
    unsigned long long txBytes;
    unsigned long long rxBytes;
    double txGBps;
    double rxGBps;
};

// Structure to hold GPU monitoring result
struct GPUMonitorResult {
    std::string gpuId;
    unsigned int nvLinkCount;  // Number of active NvLinks
    double totalTxGBps;
    double totalRxGBps;
    std::vector<NvLinkData> links;
};

/**
 * @brief NvLink Monitor class for monitoring NVIDIA NVLink bandwidth and status
 *
 * This class provides functionality to:
 * - Discover and initialize GPUs
 * - Monitor NvLink utilization
 * - Calculate bandwidth metrics
 * - Run continuous or single monitoring sessions
 */
class NvLinkMonitor {
   private:
    std::vector<GPUData> gpus;
    std::vector<GPUMonitorResult> lastResults;
    bool verboseOutput;        // Flag for detailed NvLink output
    std::ofstream outputFile;  // Output file stream
    bool fileOutput;           // Flag for file output

   public:
    /**
     * @brief Constructor - initializes NVML and discovers GPUs
     * @param verbose Enable detailed NvLink output
     * @param outputFilename Optional output file name (empty for console
     * output)
     * @throws std::runtime_error if NVML initialization fails or file cannot be
     * opened
     */
    NvLinkMonitor(bool verbose = false, const std::string& outputFilename = "");

    /**
     * @brief Destructor - shuts down NVML
     */
    ~NvLinkMonitor();

    /**
     * @brief Discovers available GPUs and their NvLink capabilities
     */
    void discoverGPUs();

    /**
     * @brief Gets current NvLink data from all GPUs
     * @return Vector of GPU monitoring results
     */
    std::vector<GPUMonitorResult> getNvLinkData();

    /**
     * @brief Calculates bandwidth between two snapshots
     * @param snapshot1 First snapshot
     * @param snapshot2 Second snapshot
     * @param timeDelta Time difference between snapshots in seconds
     * @return Vector of calculated bandwidth results
     */
    std::vector<GPUMonitorResult> calculateBandwidth(
        const std::vector<GPUMonitorResult>& snapshot1,
        const std::vector<GPUMonitorResult>& snapshot2, double timeDelta);

    /**
     * @brief Formats and prints GPU monitoring results
     * @param results Vector of GPU monitoring results to display
     */
    void formatGPUResult(const std::vector<GPUMonitorResult>& results);

    /**
     * @brief Formats and prints detailed NvLink information
     * @param results Vector of GPU monitoring results to display
     */
    void formatDetailedGPUResult(const std::vector<GPUMonitorResult>& results);

    /**
     * @brief Gets the output stream (file or console)
     * @return Reference to the output stream
     */
    std::ostream& getOutputStream();

    /**
     * @brief Runs continuous monitoring with specified interval
     * @param interval Monitoring interval in seconds
     */
    void runContinuousMonitoring(double interval);

    /**
     * @brief Runs single monitoring session
     * @param interval Interval between snapshots in seconds
     */
    void runSingleMonitoring(double interval);
};

#endif  // NVLINK_MONITOR_H