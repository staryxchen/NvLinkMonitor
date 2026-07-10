#include <gtest/gtest.h>

#include "monitor/bandwidth_calc.h"

namespace {

NvLinkData makeLink(unsigned int id, unsigned long long tx,
                    unsigned long long rx) {
    NvLinkData l;
    l.linkId = id;
    l.txBytes = tx;
    l.rxBytes = rx;
    l.txGiBps = 0.0;
    l.rxGiBps = 0.0;
    return l;
}

GPUMonitorResult makeGpu(const std::string& id, unsigned int linkCount,
                         const std::vector<NvLinkData>& links) {
    GPUMonitorResult r;
    r.gpuId = id;
    r.nvLinkCount = linkCount;
    r.totalTxGiBps = 0.0;
    r.totalRxGiBps = 0.0;
    r.links = links;
    return r;
}

}  // namespace

// 1048576 KiB = 1 GiB. Over 1 second -> 1.0 GiB/s.
TEST(CalculateBandwidth, PositiveDelta) {
    auto s1 = makeGpu("0", 1, {makeLink(0, 0, 0)});
    auto s2 = makeGpu("0", 1, {makeLink(0, 1048576ULL, 0)});
    auto r = calculateBandwidth({s1}, {s2}, 1.0, false);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_NEAR(r[0].links[0].txGiBps, 1.0, 1e-9);
    EXPECT_NEAR(r[0].links[0].rxGiBps, 0.0, 1e-9);
    EXPECT_NEAR(r[0].totalTxGiBps, 1.0, 1e-9);
    EXPECT_NEAR(r[0].totalRxGiBps, 0.0, 1e-9);
}

TEST(CalculateBandwidth, ZeroDelta) {
    auto s1 = makeGpu("0", 1, {makeLink(0, 500, 500)});
    auto s2 = makeGpu("0", 1, {makeLink(0, 500, 500)});
    auto r = calculateBandwidth({s1}, {s2}, 1.0, false);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_NEAR(r[0].links[0].txGiBps, 0.0, 1e-9);
    EXPECT_NEAR(r[0].links[0].rxGiBps, 0.0, 1e-9);
    EXPECT_NEAR(r[0].totalTxGiBps, 0.0, 1e-9);
}

TEST(CalculateBandwidth, CounterResetClampedToZero) {
    // s2 < s1 simulates a driver counter reset (the 64-bit KiB throughput
    // counters don't realistically wrap, so a negative delta means reset).
    // No meaningful rate can be computed across a reset, so it is clamped to 0.
    auto s1 = makeGpu("0", 1, {makeLink(0, 1000000, 1000000)});
    auto s2 = makeGpu("0", 1, {makeLink(0, 100, 100)});
    auto r = calculateBandwidth({s1}, {s2}, 1.0, false);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_NEAR(r[0].links[0].txGiBps, 0.0, 1e-9);
    EXPECT_NEAR(r[0].links[0].rxGiBps, 0.0, 1e-9);
    EXPECT_NEAR(r[0].totalTxGiBps, 0.0, 1e-9);
}

TEST(CalculateBandwidth, CounterResetVerboseWarningDoesNotCrash) {
    // Verbose mode emits a warning for a reset but must still produce a valid
    // zero-rate result (exercises the verbose stderr branch).
    auto s1 = makeGpu("0", 1, {makeLink(0, 1000000, 0)});
    auto s2 = makeGpu("0", 1, {makeLink(0, 100, 0)});
    auto r = calculateBandwidth({s1}, {s2}, 1.0, true);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_NEAR(r[0].links[0].txGiBps, 0.0, 1e-9);
}

TEST(CalculateBandwidth, MultiLinkAggregation) {
    auto s1 = makeGpu("0", 2, {makeLink(0, 0, 0), makeLink(1, 0, 0)});
    auto s2 = makeGpu("0", 2,
                      {makeLink(0, 1048576ULL, 0), makeLink(1, 0, 1048576ULL)});
    auto r = calculateBandwidth({s1}, {s2}, 1.0, false);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_NEAR(r[0].links[0].txGiBps, 1.0, 1e-9);
    EXPECT_NEAR(r[0].links[1].rxGiBps, 1.0, 1e-9);
    EXPECT_NEAR(r[0].totalTxGiBps, 1.0, 1e-9);
    EXPECT_NEAR(r[0].totalRxGiBps, 1.0, 1e-9);
}

TEST(CalculateBandwidth, MismatchedGpuCountDropsExtra) {
    auto s1 = makeGpu("0", 1, {makeLink(0, 0, 0)});
    auto s2a = makeGpu("0", 1, {makeLink(0, 1048576ULL, 0)});
    auto s2b = makeGpu("1", 1, {makeLink(0, 1048576ULL, 0)});
    // s2 has 2 GPUs, s1 has 1 -> second GPU dropped.
    auto r = calculateBandwidth({s1}, {s2a, s2b}, 1.0, false);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].gpuId, "0");
}

TEST(CalculateBandwidth, MismatchedLinkCountDropsExtra) {
    auto s1 = makeGpu("0", 1, {makeLink(0, 0, 0)});
    auto s2 = makeGpu("0", 2,
                      {makeLink(0, 1048576ULL, 0), makeLink(1, 1048576ULL, 0)});
    // s2 has 2 links, s1 has 1 -> second link dropped.
    auto r = calculateBandwidth({s1}, {s2}, 1.0, false);
    ASSERT_EQ(r.size(), 1u);
    ASSERT_EQ(r[0].links.size(), 1u);
    EXPECT_NEAR(r[0].totalTxGiBps, 1.0, 1e-9);
}

TEST(CalculateBandwidth, MultiGpuTotals) {
    auto s1a = makeGpu("0", 1, {makeLink(0, 0, 0)});
    auto s1b = makeGpu("1", 1, {makeLink(0, 0, 0)});
    auto s2a = makeGpu("0", 1, {makeLink(0, 1048576ULL, 0)});
    auto s2b = makeGpu("1", 1, {makeLink(0, 1048576ULL, 0)});
    auto r = calculateBandwidth({s1a, s1b}, {s2a, s2b}, 1.0, false);
    ASSERT_EQ(r.size(), 2u);
    EXPECT_NEAR(r[0].totalTxGiBps, 1.0, 1e-9);
    EXPECT_NEAR(r[1].totalTxGiBps, 1.0, 1e-9);
}

TEST(CalculateBandwidth, TimeDeltaAffectsRate) {
    // Same delta, half the time -> double the rate.
    auto s1 = makeGpu("0", 1, {makeLink(0, 0, 0)});
    auto s2 = makeGpu("0", 1, {makeLink(0, 1048576ULL, 0)});
    auto r = calculateBandwidth({s1}, {s2}, 0.5, false);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_NEAR(r[0].links[0].txGiBps, 2.0, 1e-9);
}

TEST(CalculateBandwidth, PreservesSnapshotBytes) {
    // Result should carry forward the s2 byte counters.
    auto s1 = makeGpu("0", 1, {makeLink(0, 100, 200)});
    auto s2 = makeGpu("0", 1, {makeLink(0, 1048676ULL, 1048776ULL)});
    auto r = calculateBandwidth({s1}, {s2}, 1.0, false);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].links[0].txBytes, 1048676ULL);
    EXPECT_EQ(r[0].links[0].rxBytes, 1048776ULL);
}
