#include "vk/algo/midpoint.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "util/guard_glog.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/map/context_ns.h"

namespace rayjoin::vk {

// -------------------------------------------------------------------------------
// Test Suite Setup
static std::string test_log_name = "vk_midpoint_group_test";

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

::testing::Environment* const global_env = ::testing::AddGlobalTestEnvironment(new TestEnvironment);

// -------------------------------------------------------------------------------
// Test Fixture
class TestMidPointGroupedFixture : public ::testing::Test {
 protected:
  using context_t = ContextNS<double>;
  using coord_t = typename context_t::coord_t;
  using xsect_t = typename context_t::xsect_t;
  using edge_t = typename context_t::edge_t;
  using point_t = typename context_t::point_t;

  void SetUp() override {
    auto info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string prefix = glog_guard.LogDir() + "/log-" + info->test_suite_name() + "-" + info->name();

    google::SetLogDestination(google::INFO, (prefix + ".INFO.").c_str());
    google::SetLogDestination(google::WARNING, (prefix + ".WARNING.").c_str());
    google::SetLogDestination(google::ERROR, (prefix + ".ERROR.").c_str());
    google::SetLogDestination(google::FATAL, (prefix + ".FATAL.").c_str());
  }

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

TEST_F(TestMidPointGroupedFixture, QueryMap0_AlreadySortedByEid_ReordersWithinGroupAndBuildsMidpoints) {
  // query_map_id = 0 -> groups by eid0
  //
  // Group 0: eid0 = 0, p1 = (0, 0)
  //   Unsorted by d2 on purpose:
  //     x=3 base=30
  //     x=1 base=10
  //     x=2 base=20
  //
  //   Expected reordered:
  //     x=1, x=2, x=3
  //   Expected mids:
  //     1.5, 2.5
  //
  // Group 1: eid0 = 1, p1 = (10, 0)
  //   Unsorted by d2 on purpose:
  //     x=14 base=41   d2=16
  //     x=12 base=43   d2=4
  //     x=11 base=42   d2=1
  //
  //   Expected reordered:
  //     x=11, x=12, x=14
  //   Expected mids:
  //     11.5, 13.0

  std::vector<point_t> query_points(2);
  query_points[0].x = 0.0;
  query_points[0].y = 0.0;
  query_points[1].x = 10.0;
  query_points[1].y = 0.0;

  std::vector<edge_t> query_edges(2);
  query_edges[0] = edge_t{};
  query_edges[0].p1_idx = 0;
  query_edges[1] = edge_t{};
  query_edges[1].p1_idx = 1;

  // Already sorted by query eid (eid0), but NOT sorted within each group by distance.
  std::vector<xsect_t> sorted_by_eid = {
      MakeXsect(3.0, 0.0, 0, 30),
      MakeXsect(1.0, 0.0, 0, 10),
      MakeXsect(2.0, 0.0, 0, 20),

      MakeXsect(14.0, 0.0, 1, 41),
      MakeXsect(12.0, 0.0, 1, 43),
      MakeXsect(11.0, 0.0, 1, 42),
  };

  std::vector<index_t> unique_eids = {0, 1};
  std::vector<uint32_t> xsect_index = {0, 3, 6};

  VkDeviceBuf xsects_buf;
  xsects_buf.Init(sizeof(xsect_t) * sorted_by_eid.size());
  writeToStorageBuffer(xsects_buf, sorted_by_eid);

  VkDeviceBuf unique_eids_buf;
  unique_eids_buf.Init(sizeof(index_t) * unique_eids.size());
  writeToStorageBuffer(unique_eids_buf, unique_eids);

  VkDeviceBuf xsect_index_buf;
  xsect_index_buf.Init(sizeof(uint32_t) * xsect_index.size());
  writeToStorageBuffer(xsect_index_buf, xsect_index);

  VkDeviceBuf query_edges_buf;
  query_edges_buf.Init(sizeof(edge_t) * query_edges.size());
  writeToStorageBuffer(query_edges_buf, query_edges);

  VkDeviceBuf query_points_buf;
  query_points_buf.Init(sizeof(point_t) * query_points.size());
  writeToStorageBuffer(query_points_buf, query_points);

  VkDeviceBuf reordered_xsects_buf;
  VkDeviceBuf mid_points_buf;

  const uint32_t n_mid_points = algo::ReorderXsectsAndComputeMidPoints<context_t>(xsects_buf,
                                                                                  xsect_index_buf,
                                                                                  query_edges_buf,
                                                                                  query_points_buf,
                                                                                  /*query_map_id=*/0,
                                                                                  static_cast<uint32_t>(sorted_by_eid.size()),
                                                                                  static_cast<uint32_t>(unique_eids.size()),
                                                                                  reordered_xsects_buf,
                                                                                  mid_points_buf);

  auto actual_xsects = readBackStorageBuffer<xsect_t>(reordered_xsects_buf, static_cast<uint32_t>(sorted_by_eid.size()));
  auto actual_mid_points = readBackStorageBuffer<point_t>(mid_points_buf, n_mid_points);

  ASSERT_EQ(actual_xsects.size(), sorted_by_eid.size());
  ASSERT_EQ(n_mid_points, 4u);
  ASSERT_EQ(actual_mid_points.size(), 4u);

  std::vector<xsect_t> expected_xsects = {
      MakeXsect(1.0, 0.0, 0, 10),
      MakeXsect(2.0, 0.0, 0, 20),
      MakeXsect(3.0, 0.0, 0, 30),

      MakeXsect(11.0, 0.0, 1, 42),
      MakeXsect(12.0, 0.0, 1, 43),
      MakeXsect(14.0, 0.0, 1, 41),
  };

  for (size_t i = 0; i < expected_xsects.size(); ++i) {
    EXPECT_TRUE(SameXsect(actual_xsects[i], expected_xsects[i]))
        << "Reordered xsect mismatch at i=" << i << " expected(x=" << expected_xsects[i].x << ", y=" << expected_xsects[i].y
        << ", eid0=" << expected_xsects[i].eid0 << ", eid1=" << expected_xsects[i].eid1 << ")"
        << " actual(x=" << actual_xsects[i].x << ", y=" << actual_xsects[i].y << ", eid0=" << actual_xsects[i].eid0
        << ", eid1=" << actual_xsects[i].eid1 << ")";
  }

  EXPECT_DOUBLE_EQ(actual_mid_points[0].x, 1.5);
  EXPECT_DOUBLE_EQ(actual_mid_points[0].y, 0.0);
  EXPECT_DOUBLE_EQ(actual_mid_points[1].x, 2.5);
  EXPECT_DOUBLE_EQ(actual_mid_points[1].y, 0.0);
  EXPECT_DOUBLE_EQ(actual_mid_points[2].x, 11.5);
  EXPECT_DOUBLE_EQ(actual_mid_points[2].y, 0.0);
  EXPECT_DOUBLE_EQ(actual_mid_points[3].x, 13.0);
  EXPECT_DOUBLE_EQ(actual_mid_points[3].y, 0.0);
}

TEST_F(TestMidPointGroupedFixture, QueryMap1_AlreadySortedByEid_ReordersWithinGroupAndBuildsMidpoints) {
  // query_map_id = 1 -> groups by eid1
  //
  // Group eid1=5, p1=(100,0):
  //   x=103 base(eid0)=9
  //   x=101 base(eid0)=7
  //   x=102 base(eid0)=8
  // -> reordered: 101,102,103
  // -> mids: 101.5, 102.5
  //
  // Group eid1=6, p1=(200,0):
  //   x=204 base=12
  //   x=201 base=11
  // -> reordered: 201,204
  // -> mids: 202.5

  std::vector<point_t> query_points(2);
  query_points[0].x = 100.0;
  query_points[0].y = 0.0;
  query_points[1].x = 200.0;
  query_points[1].y = 0.0;

  std::vector<edge_t> query_edges(7);
  for (auto& e: query_edges) {
    e = edge_t{};
  }
  query_edges[5].p1_idx = 0;
  query_edges[6].p1_idx = 1;

  // Already sorted by query eid (eid1)
  std::vector<xsect_t> sorted_by_eid = {
      MakeXsect(103.0, 0.0, 9, 5),
      MakeXsect(101.0, 0.0, 7, 5),
      MakeXsect(102.0, 0.0, 8, 5),

      MakeXsect(204.0, 0.0, 12, 6),
      MakeXsect(201.0, 0.0, 11, 6),
  };

  std::vector<index_t> unique_eids = {5, 6};
  std::vector<uint32_t> xsect_index = {0, 3, 5};

  VkDeviceBuf xsects_buf;
  xsects_buf.Init(sizeof(xsect_t) * sorted_by_eid.size());
  writeToStorageBuffer(xsects_buf, sorted_by_eid);

  VkDeviceBuf unique_eids_buf;
  unique_eids_buf.Init(sizeof(index_t) * unique_eids.size());
  writeToStorageBuffer(unique_eids_buf, unique_eids);

  VkDeviceBuf xsect_index_buf;
  xsect_index_buf.Init(sizeof(uint32_t) * xsect_index.size());
  writeToStorageBuffer(xsect_index_buf, xsect_index);

  VkDeviceBuf query_edges_buf;
  query_edges_buf.Init(sizeof(edge_t) * query_edges.size());
  writeToStorageBuffer(query_edges_buf, query_edges);

  VkDeviceBuf query_points_buf;
  query_points_buf.Init(sizeof(point_t) * query_points.size());
  writeToStorageBuffer(query_points_buf, query_points);

  VkDeviceBuf reordered_xsects_buf;
  VkDeviceBuf mid_points_buf;

  const uint32_t n_mid_points = algo::ReorderXsectsAndComputeMidPoints<context_t>(xsects_buf,
                                                                                  xsect_index_buf,
                                                                                  query_edges_buf,
                                                                                  query_points_buf,
                                                                                  /*query_map_id=*/1,
                                                                                  static_cast<uint32_t>(sorted_by_eid.size()),
                                                                                  static_cast<uint32_t>(unique_eids.size()),
                                                                                  reordered_xsects_buf,
                                                                                  mid_points_buf);

  auto actual_xsects = readBackStorageBuffer<xsect_t>(reordered_xsects_buf, static_cast<uint32_t>(sorted_by_eid.size()));
  auto actual_mid_points = readBackStorageBuffer<point_t>(mid_points_buf, n_mid_points);

  ASSERT_EQ(actual_xsects.size(), sorted_by_eid.size());
  ASSERT_EQ(n_mid_points, 3u);
  ASSERT_EQ(actual_mid_points.size(), 3u);

  std::vector<xsect_t> expected_xsects = {
      MakeXsect(101.0, 0.0, 7, 5),
      MakeXsect(102.0, 0.0, 8, 5),
      MakeXsect(103.0, 0.0, 9, 5),

      MakeXsect(201.0, 0.0, 11, 6),
      MakeXsect(204.0, 0.0, 12, 6),
  };

  for (size_t i = 0; i < expected_xsects.size(); ++i) {
    EXPECT_TRUE(SameXsect(actual_xsects[i], expected_xsects[i]))
        << "Reordered xsect mismatch at i=" << i << " expected(x=" << expected_xsects[i].x << ", y=" << expected_xsects[i].y
        << ", eid0=" << expected_xsects[i].eid0 << ", eid1=" << expected_xsects[i].eid1 << ")"
        << " actual(x=" << actual_xsects[i].x << ", y=" << actual_xsects[i].y << ", eid0=" << actual_xsects[i].eid0
        << ", eid1=" << actual_xsects[i].eid1 << ")";
  }

  EXPECT_DOUBLE_EQ(actual_mid_points[0].x, 101.5);
  EXPECT_DOUBLE_EQ(actual_mid_points[0].y, 0.0);
  EXPECT_DOUBLE_EQ(actual_mid_points[1].x, 102.5);
  EXPECT_DOUBLE_EQ(actual_mid_points[1].y, 0.0);
  EXPECT_DOUBLE_EQ(actual_mid_points[2].x, 202.5);
  EXPECT_DOUBLE_EQ(actual_mid_points[2].y, 0.0);
}
}  // namespace rayjoin::vk
