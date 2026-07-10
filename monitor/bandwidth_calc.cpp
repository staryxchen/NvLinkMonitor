#include "bandwidth_calc.h"

#include <iostream>

std::vector<GPUMonitorResult> calculateBandwidth(
    const std::vector<GPUMonitorResult>& snapshot1,
    const std::vector<GPUMonitorResult>& snapshot2, double timeDelta,
    bool verbose) {
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

            // Calculate byte differences (NVML returns KiB)
            long long txDelta = static_cast<long long>(link2.txBytes) -
                                static_cast<long long>(link1.txBytes);
            long long rxDelta = static_cast<long long>(link2.rxBytes) -
                                static_cast<long long>(link1.rxBytes);

            // Handle overflow cases with detailed logging
            if (txDelta < 0) {
                if (verbose) {
                    std::cerr << "Warning: TX counter overflow detected on GPU "
                              << s2.gpuId << " Link " << link2.linkId
                              << std::endl;
                }
                txDelta = 0;
            }
            if (rxDelta < 0) {
                if (verbose) {
                    std::cerr << "Warning: RX counter overflow detected on GPU "
                              << s2.gpuId << " Link " << link2.linkId
                              << std::endl;
                }
                rxDelta = 0;
            }

            // Convert KiB directly to GiB/s
            // NVML returns KiB, convert directly to GiB/s
            double txGiBps =
                static_cast<double>(txDelta) / (timeDelta * 1024.0 * 1024.0);
            double rxGiBps =
                static_cast<double>(rxDelta) / (timeDelta * 1024.0 * 1024.0);

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
