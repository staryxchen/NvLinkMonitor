#ifndef NVLINK_BW_TEST_STATS_H
#define NVLINK_BW_TEST_STATS_H

#include <cstddef>
#include <vector>

// Aggregated bandwidth statistics for a series of copy timings.
// Values are GiB/s (bufferSizeMb is MiB, divided by 1024 to get GiB).
struct BandwidthStats {
    double avgGiBps;
    double minGiBps;
    double maxGiBps;
    double avgLatencyMs;
    bool valid;
};

// Computes avg/min/max bandwidth (GiB/s) and avg latency (ms) from a
// vector of per-iteration copy times (in ms). Returns valid=false on
// empty input. min/max bandwidth are derived from max/min time
// respectively (slowest copy = lowest bandwidth).
//
// When `bidirectional` is true, bandwidth values are doubled (two buffers
// worth of data move concurrently, one in each direction); avgLatencyMs
// stays per-direction. Used by the --direction bidir test path.
BandwidthStats computeBandwidthStats(const std::vector<double>& copyTimesMs,
                                     size_t bufferSizeMb,
                                     bool bidirectional = false);

#endif  // NVLINK_BW_TEST_STATS_H
