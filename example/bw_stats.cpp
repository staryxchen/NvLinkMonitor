#include "bw_stats.h"

#include <algorithm>
#include <numeric>

BandwidthStats computeBandwidthStats(const std::vector<double>& copyTimesMs,
                                     size_t bufferSizeMb, bool bidirectional) {
    BandwidthStats stats{0.0, 0.0, 0.0, 0.0, false};

    if (copyTimesMs.empty()) {
        return stats;
    }

    double avgTime =
        std::accumulate(copyTimesMs.begin(), copyTimesMs.end(), 0.0) /
        copyTimesMs.size();
    double minTime = *std::min_element(copyTimesMs.begin(), copyTimesMs.end());
    double maxTime = *std::max_element(copyTimesMs.begin(), copyTimesMs.end());

    // bufferSizeMb is MiB; /1024.0 gives GiB. Time is ms; /1000.0 gives s.
    // In bidirectional mode two buffers move per iteration (one each way),
    // so bandwidth is doubled; latency stays per-direction.
    double bwScale = bidirectional ? 2.0 : 1.0;
    stats.avgGiBps = bwScale * (bufferSizeMb / 1024.0) / (avgTime / 1000.0);
    stats.minGiBps = bwScale * (bufferSizeMb / 1024.0) / (maxTime / 1000.0);
    stats.maxGiBps = bwScale * (bufferSizeMb / 1024.0) / (minTime / 1000.0);
    stats.avgLatencyMs = avgTime;
    stats.valid = true;

    return stats;
}
