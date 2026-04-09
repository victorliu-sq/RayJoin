#include "vk/algo/sort.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "util/guard_glog.h"
#include "vk/core/map_overlay_rt_ns.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_context.h"

namespace rayjoin::vk {

// -------------------------------------------------------------------------------
// Test Suite Setup
static std::string test_log_name = "vk_sort_xsects_test";

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
class TestSortXsectsFixture : public ::testing::Test {
 protected:
  using coord_t = double;
  using xsect_t = IntersectionNS<coord_t>;

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

  static xsect_t MakeXsect(coord_t x, coord_t y, index_t eid0, index_t eid1, polygon_id_t mid = DONTKNOW) {
    xsect_t v{};
    v.x = x;
    v.y = y;
    v.eid0 = eid0;
    v.eid1 = eid1;
    v.mid_point_polygon_id = mid;
    return v;
  }

  static bool SameXsect(const xsect_t& a, const xsect_t& b) {
    return a.x == b.x && a.y == b.y && a.eid0 == b.eid0 && a.eid1 == b.eid1 && a.mid_point_polygon_id == b.mid_point_polygon_id;
  }
};

TEST_F(TestSortXsectsFixture, SortByEid0Basic) {
  using algo::SortXsectsByQueryEid;

  std::vector<xsect_t> host_xsects = {
      MakeXsect(10.0, 1.0, 4, 100),
      MakeXsect(20.0, 2.0, 1, 200),
      MakeXsect(30.0, 3.0, 3, 300),
      MakeXsect(40.0, 4.0, 1, 400),
      MakeXsect(50.0, 5.0, 2, 500),
  };

  std::vector<xsect_t> expected = host_xsects;
  std::stable_sort(expected.begin(), expected.end(), [](const xsect_t& a, const xsect_t& b) { return a.eid0 < b.eid0; });

  VkDeviceBuf src_buf;
  src_buf.Init(sizeof(xsect_t) * host_xsects.size());
  writeToStorageBuffer(src_buf, host_xsects);

  VkDeviceBuf sorted_buf;
  SortXsectsByQueryEid<xsect_t>(src_buf, 0, static_cast<uint32_t>(host_xsects.size()), sorted_buf);

  auto actual = readBackStorageBuffer<xsect_t>(sorted_buf, static_cast<uint32_t>(host_xsects.size()));

  ASSERT_EQ(actual.size(), expected.size());

  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_TRUE(SameXsect(actual[i], expected[i]));
  }
}

}  // namespace rayjoin::vk
