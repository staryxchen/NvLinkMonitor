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
    EXPECT_NEAR(s.avgGbps, 42.857, 0.01);
    EXPECT_NEAR(s.minGbps, 25.0, 1e-9);
    EXPECT_NEAR(s.maxGbps, 100.0, 1e-9);
    EXPECT_NEAR(s.avgLatencyMs, 23.3333, 0.001);
}

TEST(BandwidthStats, SingleIteration) {
    // bufferSizeMb=1000 -> 0.9765625 GiB. Time=5ms.
    // bw = 0.9765625 / 0.005 = 195.3125 GiB/s
    auto s = computeBandwidthStats({5.0}, 1000);
    EXPECT_TRUE(s.valid);
    EXPECT_NEAR(s.avgGbps, 195.3125, 1e-9);
    EXPECT_NEAR(s.minGbps, 195.3125, 1e-9);
    EXPECT_NEAR(s.maxGbps, 195.3125, 1e-9);
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
    EXPECT_NEAR(s.avgGbps, 100.0, 1e-9);
    EXPECT_NEAR(s.minGbps, 100.0, 1e-9);
    EXPECT_NEAR(s.maxGbps, 100.0, 1e-9);
    EXPECT_NEAR(s.avgLatencyMs, 10.0, 1e-9);
}

TEST(BandwidthStats, MinMaxInversion) {
    // Slowest copy (max time) -> min bandwidth; fastest -> max bandwidth.
    auto s = computeBandwidthStats({5.0, 20.0}, 1024);
    EXPECT_TRUE(s.valid);
    // max_time=20ms -> min_bw = 1.0/0.02 = 50.0
    // min_time=5ms  -> max_bw = 1.0/0.005 = 200.0
    EXPECT_NEAR(s.minGbps, 50.0, 1e-9);
    EXPECT_NEAR(s.maxGbps, 200.0, 1e-9);
}
