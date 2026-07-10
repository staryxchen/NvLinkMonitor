#include <gtest/gtest.h>

#include <cstring>
#include <initializer_list>
#include <vector>

#include "example/arg_parser.h"

namespace {

// Builds a mutable argv from string literals for parseBwTestArgs.
struct Argv {
    std::vector<std::vector<char>> buffers;
    std::vector<char*> ptrs;
    explicit Argv(std::initializer_list<const char*> args) {
        buffers.reserve(args.size());
        for (const char* s : args) {
            buffers.emplace_back(s, s + std::strlen(s) + 1);
        }
        ptrs.reserve(args.size() + 1);
        for (auto& b : buffers) {
            ptrs.push_back(b.data());
        }
        ptrs.push_back(nullptr);
    }
    int argc() const { return static_cast<int>(ptrs.size()) - 1; }
    char** argv() { return ptrs.data(); }
};

}  // namespace

TEST(BwTestArgs, Defaults) {
    Argv a{"nvlink_bw_test"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.iterations, 100);
    EXPECT_EQ(c.buffer_size_mb, 1000u);
    EXPECT_EQ(c.src_gpu_id, 0);
    EXPECT_EQ(c.dst_gpu_id, 1);
    EXPECT_FALSE(c.help);
}

TEST(BwTestArgs, HelpShort) {
    Argv a{"nvlink_bw_test", "-h"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_TRUE(c.help);
}

TEST(BwTestArgs, HelpLong) {
    Argv a{"nvlink_bw_test", "--help"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_TRUE(c.help);
}

TEST(BwTestArgs, IterationsValid) {
    Argv a{"nvlink_bw_test", "-i", "200"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.iterations, 200);
}

TEST(BwTestArgs, IterationsZeroRejected) {
    Argv a{"nvlink_bw_test", "-i", "0"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, IterationsNegativeRejected) {
    Argv a{"nvlink_bw_test", "-i", "-1"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, IterationsNonNumericRejected) {
    Argv a{"nvlink_bw_test", "-i", "abc"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, BufferSizeValid) {
    Argv a{"nvlink_bw_test", "-b", "2000"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.buffer_size_mb, 2000u);
}

TEST(BwTestArgs, BufferSizeNegativeOneRejected) {
    // Regression: previously -1 wrapped size_t to SIZE_MAX and passed the
    // <= 0 check (unsigned), then attempted an absurd cudaMalloc.
    Argv a{"nvlink_bw_test", "-b", "-1"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, BufferSizeZeroRejected) {
    Argv a{"nvlink_bw_test", "-b", "0"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, BufferSizeNonNumericRejected) {
    Argv a{"nvlink_bw_test", "-b", "abc"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, SrcGpuValid) {
    Argv a{"nvlink_bw_test", "-s", "2"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.src_gpu_id, 2);
}

TEST(BwTestArgs, SrcGpuNegativeRejected) {
    Argv a{"nvlink_bw_test", "-s", "-1"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, DstGpuValid) {
    Argv a{"nvlink_bw_test", "-d", "3"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.dst_gpu_id, 3);
}

TEST(BwTestArgs, DstGpuNegativeRejected) {
    Argv a{"nvlink_bw_test", "-d", "-1"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, CombinedOptions) {
    Argv a{"nvlink_bw_test", "-i", "200", "-b", "2000", "-s", "0", "-d", "1"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.iterations, 200);
    EXPECT_EQ(c.buffer_size_mb, 2000u);
    EXPECT_EQ(c.src_gpu_id, 0);
    EXPECT_EQ(c.dst_gpu_id, 1);
}

TEST(BwTestArgs, UnknownOptionRejected) {
    Argv a{"nvlink_bw_test", "--bogus"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_FALSE(c.ok);
    EXPECT_FALSE(c.errorMessage.empty());
}

TEST(BwTestArgs, LongOptions) {
    Argv a{"nvlink_bw_test",
           "--iterations",
           "50",
           "--buffer-size",
           "500",
           "--src-gpu",
           "0",
           "--dst-gpu",
           "2"};
    auto c = parseBwTestArgs(a.argc(), a.argv());
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.iterations, 50);
    EXPECT_EQ(c.buffer_size_mb, 500u);
    EXPECT_EQ(c.src_gpu_id, 0);
    EXPECT_EQ(c.dst_gpu_id, 2);
}
