#ifndef RAYJOIN_RADIX_SORT_H
#define RAYJOIN_RADIX_SORT_H

#include <cstdint>
#include <string>
#include <utility>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_engine.h"
#include "vk/util/type_native.h"


namespace rayjoin::vk {
namespace algo {

inline uint32_t RadixSortDivUpU32(uint32_t a, uint32_t b) { return (a + b - 1u) / b; }

template<IntersectionNSType XsectT>
inline void RadixSortXsectsByQueryEid(const VkDeviceBuf& src_xsects_buf, int32_t query_map_id, uint32_t count, VkDeviceBuf& dst_sorted_xsects_buf) {
  if (count == 0u) {
    dst_sorted_xsects_buf = VkDeviceBuf{};
    return;
  }

  constexpr uint32_t kThreadsPerBlock = 256u;
  constexpr uint32_t kItemsPerThread = 4u;
  constexpr uint32_t kItemsPerBlock = kThreadsPerBlock * kItemsPerThread;  // 1024
  constexpr uint32_t kRadixBits = 4u;
  constexpr uint32_t kRadix = 1u << kRadixBits;  // 16
  constexpr uint32_t kPasses = 32u / kRadixBits;  // 8

  const uint32_t num_blocks = RadixSortDivUpU32(count, kItemsPerBlock);

  const std::string histogram_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_histogram_ns.spv";
  const std::string scan_offsets_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_scan_offsets_ns.spv";
  const std::string scatter_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_scatter_ns.spv";

  VkDeviceBuf ping_buf;
  VkDeviceBuf pong_buf;
  ping_buf.Init(sizeof(XsectT) * count);
  pong_buf.Init(sizeof(XsectT) * count);

  copyDeviceBuffer(src_xsects_buf, ping_buf, sizeof(XsectT) * static_cast<VkDeviceSize>(count));

  VkDeviceBuf* src_buf = &ping_buf;
  VkDeviceBuf* dst_buf = &pong_buf;

  for (uint32_t pass = 0; pass < kPasses; ++pass) {
    const uint32_t bit_shift = pass * kRadixBits;

    VkDeviceBuf block_hist_buf;
    VkDeviceBuf block_offsets_buf;
    VkDeviceBuf bucket_bases_buf;

    block_hist_buf.Init(sizeof(uint32_t) * kRadix * num_blocks);
    block_offsets_buf.Init(sizeof(uint32_t) * kRadix * num_blocks);
    bucket_bases_buf.Init(sizeof(uint32_t) * kRadix);

    {
      struct LaunchParamsHistogram {
        int32_t query_map_id;
        uint32_t count;
        uint32_t bit_shift;
        uint32_t num_blocks;
      };

      RunComputePass(num_blocks * kThreadsPerBlock,
                     histogram_spv.c_str(),
                     LaunchParamsHistogram{
                         .query_map_id = query_map_id,
                         .count = count,
                         .bit_shift = bit_shift,
                         .num_blocks = num_blocks,
                     },
                     *src_buf,  // binding 0 -> gXsectsIn
                     block_hist_buf);  // binding 1 -> gBlockHist
    }

    {
      struct LaunchParamsScanOffsets {
        uint32_t num_blocks;
        uint32_t _pad0;
        uint32_t _pad1;
        uint32_t _pad2;
      };

      RunComputePass(256u,
                     scan_offsets_spv.c_str(),
                     LaunchParamsScanOffsets{
                         .num_blocks = num_blocks,
                         ._pad0 = 0u,
                         ._pad1 = 0u,
                         ._pad2 = 0u,
                     },
                     block_hist_buf,  // binding 0 -> gBlockHist
                     block_offsets_buf,  // binding 1 -> gBlockOffsets
                     bucket_bases_buf);  // binding 2 -> gBucketBases
    }

    {
      struct LaunchParamsScatter {
        int32_t query_map_id;
        uint32_t count;
        uint32_t bit_shift;
        uint32_t num_blocks;
      };

      RunComputePass(num_blocks * kThreadsPerBlock,
                     scatter_spv.c_str(),
                     LaunchParamsScatter{
                         .query_map_id = query_map_id,
                         .count = count,
                         .bit_shift = bit_shift,
                         .num_blocks = num_blocks,
                     },
                     *src_buf,  // binding 0 -> gXsectsIn
                     *dst_buf,  // binding 1 -> gXsectsOut
                     block_offsets_buf,  // binding 2 -> gBlockOffsets
                     bucket_bases_buf);  // binding 3 -> gBucketBases
    }

    std::swap(src_buf, dst_buf);
  }

  dst_sorted_xsects_buf = std::move(*src_buf);
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_RADIX_SORT_H
