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
        result.totalTxGiBps = 0.0;
        result.totalRxGiBps = 0.0;

        for (size_t j = 0; j < s2.links.size(); j++) {
            if (j >= s1.links.size()) continue;

            const auto& link1 = s1.links[j];
            const auto& link2 = s2.links[j];

            // Calculate byte differences (NVML returns KiB)
            long long txDelta = static_cast<long long>(link2.txBytes) -
                                static_cast<long long>(link1.txBytes);
            long long rxDelta = static_cast<long long>(link2.rxBytes) -
                                static_cast<long long>(link1.rxBytes);

            // A negative delta means the counter went backwards. The NVML
            // throughput counters are 64-bit KiB values (~8 EiB wrap range),
            // so genuine arithmetic overflow is effectively impossible — a
            // negative delta almost always indicates the driver reset the
            // counter (e.g. on certain driver events). No meaningful rate can
            // be computed across a reset, so we clamp to 0 for this sample
            // and warn so the user can filter reset samples out of steady-
            // state averages rather than treating them as idle links.
            if (txDelta < 0) {
                if (verbose) {
                    std::cerr << "Warning: TX counter reset detected on GPU "
                              << s2.gpuId << " Link " << link2.linkId
                              << " (delta=" << txDelta
                              << "); sample rate set to 0" << std::endl;
                }
                txDelta = 0;
            }
            if (rxDelta < 0) {
                if (verbose) {
                    std::cerr << "Warning: RX counter reset detected on GPU "
                              << s2.gpuId << " Link " << link2.linkId
                              << " (delta=" << rxDelta
                              << "); sample rate set to 0" << std::endl;
                }
                rxDelta = 0;
            }

            // Convert KiB directly to GiB/s (NVML counters are in KiB)
            double txRate =
                static_cast<double>(txDelta) / (timeDelta * 1024.0 * 1024.0);
            double rxRate =
                static_cast<double>(rxDelta) / (timeDelta * 1024.0 * 1024.0);

            NvLinkData linkData;
            linkData.linkId = link2.linkId;
            linkData.txGiBps = txRate;
            linkData.rxGiBps = rxRate;
            linkData.txBytes = link2.txBytes;
            linkData.rxBytes = link2.rxBytes;

            result.links.push_back(linkData);
            result.totalTxGiBps += txRate;
            result.totalRxGiBps += rxRate;
        }

        results.push_back(result);
    }

    return results;
}
