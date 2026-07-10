#ifndef NVLINK_MONITOR_BANDWIDTH_CALC_H
#define NVLINK_MONITOR_BANDWIDTH_CALC_H

#include <vector>

#include "nvlink_monitor.h"

// Calculates bandwidth between two NVML counter snapshots.
//
// Counters are in KiB; results are stored in the txGBps/rxGBps fields
// as GiB/s (the field names retain the legacy "GBps" naming despite
// holding GiB/s values). Negative deltas from counter overflow are
// clamped to 0, with an optional stderr warning when verbose == true.
std::vector<GPUMonitorResult> calculateBandwidth(
    const std::vector<GPUMonitorResult>& snapshot1,
    const std::vector<GPUMonitorResult>& snapshot2, double timeDelta,
    bool verbose);

#endif  // NVLINK_MONITOR_BANDWIDTH_CALC_H
