#include "vk/algo/exclusive_scan.h"

#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "util/guard_glog.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_context.h"

namespace rayjoin::vk {

// -------------------------------------------------------------------------------
// Test Suite Setup
static std::string test_log_name = "vk_scan_uint32_test";

// -------------------------------------------------------------------------------
// Glog Wrapper
static GlogGuard glog_guard = CreateGlogGuardAlsoToStderr(test_log_name.c_str());

// Vulkan Runtime
static VkGlobalRuntime vk_runtime = CreateVkGlobalRuntime();

// -------------------------------------------------------------------------------
// Test Environment
class TestEnvironment : public ::testing::Environment {
 public:
  ~TestEnvironment() override = default;
  void SetUp() override {}
  void TearDown() override {}
};

// Register the environment before any tests run
::testing::Environment* const global_env = ::testing::AddGlobalTestEnvironment(new TestEnvironment);

// -------------------------------------------------------------------------------
// Test Fixture
class TestExclusiveScanUInt32Fixture : public ::testing::Test {
 protected:
  void SetUp() override {
    auto info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_suite = info->test_suite_name();
    std::string test_name = info->name();
    std::string prefix = glog_guard.LogDir() + "/log-" + test_suite + "-" + test_name;

    google::SetLogDestination(google::INFO, (prefix + ".INFO.").c_str());
    google::SetLogDestination(google::WARNING, (prefix + ".WARNING.").c_str());
    google::SetLogDestination(google::ERROR, (prefix + ".ERROR.").c_str());
    google::SetLogDestination(google::FATAL, (prefix + ".FATAL.").c_str());
  }

  void TearDown() override {}

  static std::vector<uint32_t> CpuExclusiveScan(const std::vector<uint32_t>& in) {
    std::vector<uint32_t> out(in.size(), 0u);
    uint32_t sum = 0u;
    for (size_t i = 0; i < in.size(); ++i) {
      out[i] = sum;
      sum += in[i];
    }
    return out;
  }
};

TEST_F(TestExclusiveScanUInt32Fixture, BasicSmallExample) {
  std::vector<uint32_t> host_in = {1u, 0u, 0u, 1u, 0u, 1u};
  std::vector<uint32_t> expected = {0u, 1u, 1u, 1u, 2u, 2u};

  VkDeviceBuf in_buf;
  in_buf.Init(sizeof(uint32_t) * host_in.size());
  writeToStorageBuffer(in_buf, host_in);

  VkDeviceBuf out_buf;
  out_buf.Init(sizeof(uint32_t) * host_in.size());

  algo::ExclusiveScanUInt32(in_buf, out_buf, static_cast<uint32_t>(host_in.size()));

  auto actual = readBackStorageBuffer<uint32_t>(out_buf, static_cast<uint32_t>(host_in.size()));

  ASSERT_EQ(actual.size(), expected.size());

  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_EQ(actual[i], expected[i]) << "Mismatch at i=" << i << " expected=" << expected[i] << " actual=" << actual[i];
  }
}

TEST_F(TestExclusiveScanUInt32Fixture, Random100) {
  std::mt19937 rng(123456u);
  std::uniform_int_distribution<uint32_t> dist(0u, 9u);

  std::vector<uint32_t> host_in;
  host_in.reserve(100);
  for (int i = 0; i < 100; ++i) {
    host_in.push_back(dist(rng));
  }

  std::vector<uint32_t> expected = CpuExclusiveScan(host_in);

  VkDeviceBuf in_buf;
  in_buf.Init(sizeof(uint32_t) * host_in.size());
  writeToStorageBuffer(in_buf, host_in);

  VkDeviceBuf out_buf;
  out_buf.Init(sizeof(uint32_t) * host_in.size());

  algo::ExclusiveScanUInt32(in_buf, out_buf, static_cast<uint32_t>(host_in.size()));

  auto actual = readBackStorageBuffer<uint32_t>(out_buf, static_cast<uint32_t>(host_in.size()));

  ASSERT_EQ(actual.size(), expected.size());

  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_EQ(actual[i], expected[i]) << "Mismatch at i=" << i << " expected=" << expected[i] << " actual=" << actual[i];
  }
}

}  // namespace rayjoin::vk
