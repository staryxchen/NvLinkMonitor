#include <gtest/gtest.h>

#include <cstring>
#include <initializer_list>
#include <vector>

#include "monitor/arg_parser.h"

namespace {

// Builds a mutable argv from string literals for parseMonitorArgs.
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

TEST(MonitorArgs, Defaults) {
    Argv a{"nvlink_monitor"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_DOUBLE_EQ(args.interval, 1.0);
    EXPECT_TRUE(args.continuous);
    EXPECT_FALSE(args.verbose);
    EXPECT_EQ(args.format, OutputFormat::Text);
    EXPECT_TRUE(args.outputFilename.empty());
    EXPECT_FALSE(args.helpRequested);
}

TEST(MonitorArgs, HelpShort) {
    Argv a{"nvlink_monitor", "-h"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.helpRequested);
    EXPECT_TRUE(args.ok);
}

TEST(MonitorArgs, HelpLong) {
    Argv a{"nvlink_monitor", "--help"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.helpRequested);
}

TEST(MonitorArgs, VerboseShort) {
    Argv a{"nvlink_monitor", "-v"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.verbose);
}

TEST(MonitorArgs, VerboseLong) {
    Argv a{"nvlink_monitor", "--verbose"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.verbose);
}

TEST(MonitorArgs, IntervalValue) {
    Argv a{"nvlink_monitor", "-i", "0.5"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_DOUBLE_EQ(args.interval, 0.5);
}

TEST(MonitorArgs, IntervalLong) {
    Argv a{"nvlink_monitor", "--interval", "2.0"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_DOUBLE_EQ(args.interval, 2.0);
}

TEST(MonitorArgs, IntervalZeroRejected) {
    Argv a{"nvlink_monitor", "-i", "0"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
    EXPECT_FALSE(args.errorMessage.empty());
}

TEST(MonitorArgs, IntervalNegativeRejected) {
    Argv a{"nvlink_monitor", "-i", "-1"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
}

TEST(MonitorArgs, IntervalNonNumericRejected) {
    Argv a{"nvlink_monitor", "-i", "abc"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
}

TEST(MonitorArgs, IntervalMissingValue) {
    Argv a{"nvlink_monitor", "-i"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
}

TEST(MonitorArgs, OutputFile) {
    Argv a{"nvlink_monitor", "-o", "out.log"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_EQ(args.outputFilename, "out.log");
}

TEST(MonitorArgs, OutputLong) {
    Argv a{"nvlink_monitor", "--output", "out.log"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_EQ(args.outputFilename, "out.log");
}

TEST(MonitorArgs, OutputMissingFilename) {
    Argv a{"nvlink_monitor", "-o"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
}

TEST(MonitorArgs, ContinuousNoValueDefaultsTrue) {
    Argv a{"nvlink_monitor", "-c"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_TRUE(args.continuous);
}

TEST(MonitorArgs, ContinuousLongNoValue) {
    Argv a{"nvlink_monitor", "--continuous"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_TRUE(args.continuous);
}

TEST(MonitorArgs, ContinuousFalse) {
    Argv a{"nvlink_monitor", "--continuous", "false"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_FALSE(args.continuous);
}

TEST(MonitorArgs, ContinuousTrue) {
    Argv a{"nvlink_monitor", "-c", "true"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_TRUE(args.continuous);
}

TEST(MonitorArgs, FormatDefaultIsText) {
    Argv a{"nvlink_monitor"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_EQ(args.format, OutputFormat::Text);
}

TEST(MonitorArgs, FormatTextExplicit) {
    Argv a{"nvlink_monitor", "--format", "text"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_EQ(args.format, OutputFormat::Text);
}

TEST(MonitorArgs, FormatCsvShort) {
    Argv a{"nvlink_monitor", "-f", "csv"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_EQ(args.format, OutputFormat::CSV);
}

TEST(MonitorArgs, FormatCsvLong) {
    Argv a{"nvlink_monitor", "--format", "csv"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_EQ(args.format, OutputFormat::CSV);
}

TEST(MonitorArgs, FormatJsonLong) {
    Argv a{"nvlink_monitor", "--format", "json"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_EQ(args.format, OutputFormat::JSON);
}

TEST(MonitorArgs, FormatInvalidRejected) {
    Argv a{"nvlink_monitor", "--format", "yaml"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
    EXPECT_FALSE(args.errorMessage.empty());
}

TEST(MonitorArgs, FormatMissingValueRejected) {
    Argv a{"nvlink_monitor", "--format"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
}

TEST(MonitorArgs, UnknownOptionRejected) {
    Argv a{"nvlink_monitor", "--bogus"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_FALSE(args.ok);
}

TEST(MonitorArgs, CombinedOptions) {
    Argv a{"nvlink_monitor", "-v",       "-i", "0.5", "-o",
           "out.log",        "--format", "csv"};
    auto args = parseMonitorArgs(a.argc(), a.argv());
    EXPECT_TRUE(args.ok);
    EXPECT_TRUE(args.verbose);
    EXPECT_DOUBLE_EQ(args.interval, 0.5);
    EXPECT_EQ(args.outputFilename, "out.log");
    EXPECT_EQ(args.format, OutputFormat::CSV);
}
