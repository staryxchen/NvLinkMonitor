#include <gtest/gtest.h>

#include <vector>

#include "example/bw_stats.h"

TEST(BandwidthStats, KnownValues) {
    // bufferSizeMb=1024 -> 1.0 GiB. Times: {10,20,40} ms.
    // avg=23.333ms -> avg_bw = 1.0/0.023333 = 42.857 GiB/s
    // min_time=10ms -> max_bw = 1.0/0.01 = 100.0
    // max_time=40ms -> min_bw = 1.0/0.04 = 25.0
    auto s = computeBandwidthStats({10.0, 20.0, 40.0}, 1024);
    EXPECT_TRUE(s.valid);
    EXPECT_NEAR(s.avgGiBps, 42.857, 0.01);
    EXPECT_NEAR(s.minGiBps, 25.0, 1e-9);
    EXPECT_NEAR(s.maxGiBps, 100.0, 1e-9);
    EXPECT_NEAR(s.avgLatencyMs, 23.3333, 0.001);
}

TEST(BandwidthStats, SingleIteration) {
    // bufferSizeMb=1000 -> 0.9765625 GiB. Time=5ms.
    // bw = 0.9765625 / 0.005 = 195.3125 GiB/s
    auto s = computeBandwidthStats({5.0}, 1000);
    EXPECT_TRUE(s.valid);
    EXPECT_NEAR(s.avgGiBps, 195.3125, 1e-9);
    EXPECT_NEAR(s.minGiBps, 195.3125, 1e-9);
    EXPECT_NEAR(s.maxGiBps, 195.3125, 1e-9);
    EXPECT_NEAR(s.avgLatencyMs, 5.0, 1e-9);
}

TEST(BandwidthStats, EmptyInput) {
    auto s = computeBandwidthStats({}, 1024);
    EXPECT_FALSE(s.valid);
}

TEST(BandwidthStats, AllEqualTimings) {
    // 1.0 GiB / 0.01s = 100.0 GiB/s for all three.
    auto s = computeBandwidthStats({10.0, 10.0, 10.0}, 1024);
    EXPECT_TRUE(s.valid);
    EXPECT_NEAR(s.avgGiBps, 100.0, 1e-9);
    EXPECT_NEAR(s.minGiBps, 100.0, 1e-9);
    EXPECT_NEAR(s.maxGiBps, 100.0, 1e-9);
    EXPECT_NEAR(s.avgLatencyMs, 10.0, 1e-9);
}

TEST(BandwidthStats, MinMaxInversion) {
    // Slowest copy (max time) -> min bandwidth; fastest -> max bandwidth.
    auto s = computeBandwidthStats({5.0, 20.0}, 1024);
    EXPECT_TRUE(s.valid);
    // max_time=20ms -> min_bw = 1.0/0.02 = 50.0
    // min_time=5ms  -> max_bw = 1.0/0.005 = 200.0
    EXPECT_NEAR(s.minGiBps, 50.0, 1e-9);
    EXPECT_NEAR(s.maxGiBps, 200.0, 1e-9);
}

TEST(BandwidthStats, BidirectionalDoublesBandwidth) {
    // Same timings as KnownValues; bidirectional should double the bandwidth
    // while leaving per-direction latency unchanged.
    auto unidir = computeBandwidthStats({10.0, 20.0, 40.0}, 1024, false);
    auto bidir = computeBandwidthStats({10.0, 20.0, 40.0}, 1024, true);
    EXPECT_TRUE(unidir.valid);
    EXPECT_TRUE(bidir.valid);
    EXPECT_NEAR(bidir.avgGiBps, unidir.avgGiBps * 2.0, 1e-6);
    EXPECT_NEAR(bidir.minGiBps, unidir.minGiBps * 2.0, 1e-6);
    EXPECT_NEAR(bidir.maxGiBps, unidir.maxGiBps * 2.0, 1e-6);
    // Latency is per-direction and must NOT be doubled.
    EXPECT_NEAR(bidir.avgLatencyMs, unidir.avgLatencyMs, 1e-9);
}

TEST(BandwidthStats, BidirectionalEmpty) {
    auto s = computeBandwidthStats({}, 1024, true);
    EXPECT_FALSE(s.valid);
}
