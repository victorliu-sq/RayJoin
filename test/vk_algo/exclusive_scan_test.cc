#include "vk/algo/exclusive_scan.h"

#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "test_vk_fixture.h"
#include "util/guard_glog.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_context.h"

namespace rayjoin::vk {

class TestExclusiveScanUInt32Fixture : public TestVkFixture {
 protected:
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
