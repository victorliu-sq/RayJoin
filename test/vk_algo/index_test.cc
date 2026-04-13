#include "vk/algo/index.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "test_vk_fixture.h"
#include "util/guard_glog.h"
#include "vk/algo/index.h"
#include "vk/algo/sort.h"
#include "vk/algo/unique.h"
#include "vk/core/map_overlay_rt_ns.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_context.h"

namespace rayjoin::vk {

class TestBuildXsectIndexFixture : public TestVkFixture {
 protected:
  static std::vector<uint32_t> CpuReferenceIndex(std::vector<xsect_t> xs, int32_t query_map_id) {
    auto query_eid_of = [query_map_id](const xsect_t& x) -> index_t { return (query_map_id == 0) ? x.eid0 : x.eid1; };

    std::stable_sort(xs.begin(), xs.end(), [&](const xsect_t& a, const xsect_t& b) { return query_eid_of(a) < query_eid_of(b); });

    std::vector<index_t> unique_eids;
    unique_eids.reserve(xs.size());

    for (const auto& x: xs) {
      const index_t eid = query_eid_of(x);
      if (unique_eids.empty() || unique_eids.back() != eid) {
        unique_eids.push_back(eid);
      }
    }

    std::vector<uint32_t> xsect_index(unique_eids.size() + 1u, 0u);
    size_t pos = 0;
    size_t group_idx = 0;

    while (pos < xs.size()) {
      const index_t eid = query_eid_of(xs[pos]);
      size_t end = pos + 1;
      while (end < xs.size() && query_eid_of(xs[end]) == eid) {
        ++end;
      }

      xsect_index[group_idx + 1] = xsect_index[group_idx] + static_cast<uint32_t>(end - pos);
      pos = end;
      ++group_idx;
    }

    return xsect_index;
  }
};

TEST_F(TestBuildXsectIndexFixture, BasicQueryMap1) {
  std::vector<xsect_t> host_xsects = {
      MakeXsect(10.0, 1.0, 100, 4),
      MakeXsect(20.0, 2.0, 200, 1),
      MakeXsect(30.0, 3.0, 300, 3),
      MakeXsect(40.0, 4.0, 400, 1),
      MakeXsect(50.0, 5.0, 500, 2),
      MakeXsect(60.0, 6.0, 600, 2),
      MakeXsect(70.0, 7.0, 700, 4),
  };

  const int32_t query_map_id = 1;
  const auto expected = CpuReferenceIndex(host_xsects, query_map_id);

  VkDeviceBuf src_buf;
  src_buf.Init(sizeof(xsect_t) * host_xsects.size());
  writeToStorageBuffer(src_buf, host_xsects);

  VkDeviceBuf sorted_buf;
  algo::SortXsectsByQueryEid<xsect_t>(src_buf, query_map_id, static_cast<uint32_t>(host_xsects.size()), sorted_buf);

  VkDeviceBuf unique_eids_buf;
  VkDeviceBuf unique_count_buf;
  algo::DedupSortedXsectsToUniqueEids(sorted_buf, query_map_id, static_cast<uint32_t>(host_xsects.size()), unique_eids_buf, unique_count_buf);

  const uint32_t unique_count = readBackStorageBuffer<uint32_t>(unique_count_buf);

  VkDeviceBuf xsect_index_buf;
  algo::BuildXsectIndexFromSortedXsects(
      sorted_buf, unique_eids_buf, query_map_id, static_cast<uint32_t>(host_xsects.size()), unique_count, xsect_index_buf);

  const auto actual = readBackStorageBuffer<uint32_t>(xsect_index_buf, unique_count + 1u);

  ASSERT_EQ(actual.size(), expected.size());

  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_EQ(actual[i], expected[i]) << "Mismatch at i=" << i << " expected=" << expected[i] << " actual=" << actual[i];
  }
}

TEST_F(TestBuildXsectIndexFixture, Random100QueryMap1) {
  std::mt19937 rng(123456u);
  std::uniform_int_distribution<index_t> eid_dist(0, 99);

  std::vector<xsect_t> host_xsects;
  host_xsects.reserve(100);

  for (int i = 0; i < 100; ++i) {
    host_xsects.push_back(MakeXsect(static_cast<coord_t>(i), static_cast<coord_t>(100 - i), eid_dist(rng), eid_dist(rng)));
  }

  const int32_t query_map_id = 1;
  const auto expected = CpuReferenceIndex(host_xsects, query_map_id);

  VkDeviceBuf src_buf;
  src_buf.Init(sizeof(xsect_t) * host_xsects.size());
  writeToStorageBuffer(src_buf, host_xsects);

  VkDeviceBuf sorted_buf;
  algo::SortXsectsByQueryEid<xsect_t>(src_buf, query_map_id, static_cast<uint32_t>(host_xsects.size()), sorted_buf);

  VkDeviceBuf unique_eids_buf;
  VkDeviceBuf unique_count_buf;
  algo::DedupSortedXsectsToUniqueEids(sorted_buf, query_map_id, static_cast<uint32_t>(host_xsects.size()), unique_eids_buf, unique_count_buf);

  const uint32_t unique_count = readBackStorageBuffer<uint32_t>(unique_count_buf);

  VkDeviceBuf xsect_index_buf;
  algo::BuildXsectIndexFromSortedXsects(
      sorted_buf, unique_eids_buf, query_map_id, static_cast<uint32_t>(host_xsects.size()), unique_count, xsect_index_buf);

  const auto actual = readBackStorageBuffer<uint32_t>(xsect_index_buf, unique_count + 1u);

  ASSERT_EQ(actual.size(), expected.size());

  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_EQ(actual[i], expected[i]) << "Mismatch at i=" << i << " expected=" << expected[i] << " actual=" << actual[i];
  }
}

}  // namespace rayjoin::vk
