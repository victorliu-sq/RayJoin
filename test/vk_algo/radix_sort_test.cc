#include "vk/algo/radix_sort.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

#include "test_vk_fixture.h"
#include "util/guard_glog.h"
#include "vk/core/map_overlay_rt_ns.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_context.h"

namespace rayjoin::vk {

class TestRadixSortXsectsFixture : public TestVkFixture {
 protected:
  static void CheckRadixSortXsectsByQueryEid(const std::vector<xsect_t>& host_xsects, int32_t query_map_id) {
    ASSERT_TRUE(query_map_id == 0 || query_map_id == 1);

    VkDeviceBuf src_buf;
    src_buf.Init(sizeof(xsect_t) * host_xsects.size());
    writeToStorageBuffer(src_buf, host_xsects);

    std::vector<xsect_t> expected = host_xsects;
    std::stable_sort(expected.begin(), expected.end(), [query_map_id](const xsect_t& a, const xsect_t& b) {
      return (query_map_id == 0) ? (a.eid0 < b.eid0) : (a.eid1 < b.eid1);
    });

    VkDeviceBuf sorted_buf;
    algo::RadixSortXsectsByQueryEid<xsect_t>(src_buf, query_map_id, static_cast<uint32_t>(host_xsects.size()), sorted_buf);

    auto actual = readBackStorageBuffer<xsect_t>(sorted_buf, static_cast<uint32_t>(host_xsects.size()));

    ASSERT_EQ(actual.size(), expected.size());

    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_TRUE(SameXsect(actual[i], expected[i]))
          << "Mismatch at index " << i << " for query_map_id=" << query_map_id << " expected eid0=" << expected[i].eid0
          << " expected eid1=" << expected[i].eid1 << " actual eid0=" << actual[i].eid0 << " actual eid1=" << actual[i].eid1;
    }
  }
};

TEST_F(TestRadixSortXsectsFixture, RadixSortXsectsByQueryEidMatchesCpuStableSort) {
  std::mt19937 rng(123456u);
  std::uniform_int_distribution<uint32_t> eid_dist(0u, 31u);

  std::vector<xsect_t> host_xsects;
  host_xsects.reserve(256);

  for (uint32_t i = 0; i < 256u; ++i) {
    const coord_t x = static_cast<coord_t>(i);
    const coord_t y = static_cast<coord_t>(1000u - i);
    const uint32_t eid0 = eid_dist(rng);
    const uint32_t eid1 = eid_dist(rng);
    host_xsects.push_back(MakeXsect(x, y, eid0, eid1));
  }

  CheckRadixSortXsectsByQueryEid(host_xsects, /*query_map_id=*/0);
  CheckRadixSortXsectsByQueryEid(host_xsects, /*query_map_id=*/1);
}

TEST_F(TestRadixSortXsectsFixture, RadixSortXsectsByQueryEidMatchesCpuStableSort_Large) {
  constexpr uint32_t kCount = 16384u;

  std::mt19937 rng(123456u);
  std::uniform_int_distribution<uint32_t> eid_dist(0u, 2047u);

  std::vector<xsect_t> host_xsects;
  host_xsects.reserve(kCount);

  for (uint32_t i = 0; i < kCount; ++i) {
    const coord_t x = static_cast<coord_t>(i);
    const coord_t y = static_cast<coord_t>(kCount - i);
    const uint32_t eid0 = eid_dist(rng);
    const uint32_t eid1 = eid_dist(rng);
    host_xsects.push_back(MakeXsect(x, y, eid0, eid1));
  }

  CheckRadixSortXsectsByQueryEid(host_xsects, /*query_map_id=*/0);
  CheckRadixSortXsectsByQueryEid(host_xsects, /*query_map_id=*/1);
}

}  // namespace rayjoin::vk
