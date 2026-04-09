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
};

TEST_F(TestSortXsectsFixture, SortByEid1Basic) {
  std::vector<xsect_t> host_xsects = {
      MakeXsect(10.0, 1.0, 100, 4),
      MakeXsect(20.0, 2.0, 200, 1),
      MakeXsect(30.0, 3.0, 300, 3),
      MakeXsect(40.0, 4.0, 400, 1),
      MakeXsect(50.0, 5.0, 500, 2),
  };

  VkDeviceBuf src_buf;
  src_buf.Init(sizeof(xsect_t) * host_xsects.size());
  writeToStorageBuffer(src_buf, host_xsects);

  VkDeviceBuf sorted_buf;
  algo::SortXsectsByQueryEid<xsect_t>(src_buf,
                                      /*query_map_id=*/1,
                                      static_cast<uint32_t>(host_xsects.size()),
                                      sorted_buf);

  auto actual = readBackStorageBuffer<xsect_t>(sorted_buf, static_cast<uint32_t>(host_xsects.size()));

  ASSERT_EQ(actual.size(), host_xsects.size());

  EXPECT_TRUE(std::is_sorted(actual.begin(), actual.end(), [](const xsect_t& a, const xsect_t& b) { return a.eid1 < b.eid1; }));
}

TEST_F(TestSortXsectsFixture, SortByEid1Random100) {
  std::mt19937 rng(123456u);
  std::uniform_int_distribution<index_t> eid_dist(0, 99);

  std::vector<xsect_t> host_xsects;
  host_xsects.reserve(100);

  for (int i = 0; i < 100; ++i) {
    const coord_t x = static_cast<coord_t>(i);
    const coord_t y = static_cast<coord_t>(100 - i);
    const index_t eid0 = eid_dist(rng);
    const index_t eid1 = eid_dist(rng);
    host_xsects.push_back(MakeXsect(x, y, eid0, eid1));
  }

  VkDeviceBuf src_buf;
  src_buf.Init(sizeof(xsect_t) * host_xsects.size());
  writeToStorageBuffer(src_buf, host_xsects);

  VkDeviceBuf sorted_buf;
  algo::SortXsectsByQueryEid<xsect_t>(src_buf,
                                      /*query_map_id=*/1,
                                      static_cast<uint32_t>(host_xsects.size()),
                                      sorted_buf);

  auto actual = readBackStorageBuffer<xsect_t>(sorted_buf, static_cast<uint32_t>(host_xsects.size()));

  ASSERT_EQ(actual.size(), host_xsects.size());

  EXPECT_TRUE(std::is_sorted(actual.begin(), actual.end(), [](const xsect_t& a, const xsect_t& b) { return a.eid1 < b.eid1; }));
}

}  // namespace rayjoin::vk
