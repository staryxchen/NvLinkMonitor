#ifndef NVLINK_BW_TEST_STATS_H
#define NVLINK_BW_TEST_STATS_H

#include <cstddef>
#include <vector>

// Aggregated bandwidth statistics for a series of copy timings.
//
// Note: field names use "Gbps" for consistency with the existing
// output text, but values are GiB/s (buffer_size_mb is MiB, divided
// by 1024 to get GiB).
struct BandwidthStats {
    double avgGbps;
    double minGbps;
    double maxGbps;
    double avgLatencyMs;
    bool valid;
};

// Computes avg/min/max bandwidth (GiB/s) and avg latency (ms) from a
// vector of per-iteration copy times (in ms). Returns valid=false on
// empty input. min/max bandwidth are derived from max/min time
// respectively (slowest copy = lowest bandwidth).
BandwidthStats computeBandwidthStats(const std::vector<double>& copyTimesMs,
                                     size_t bufferSizeMb);

#endif  // NVLINK_BW_TEST_STATS_H
