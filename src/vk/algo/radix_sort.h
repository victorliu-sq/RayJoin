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

template<IntersectionNSType XsectT>
inline void RadixSortXsectsByQueryEid(const VkDeviceBuf& src_xsects_buf, int32_t query_map_id, uint32_t count, VkDeviceBuf& dst_sorted_xsects_buf) {
  static_assert(std::is_standard_layout_v<XsectT>, "XsectT must be standard-layout");
  static_assert(sizeof(decltype(std::declval<XsectT>().eid0)) == sizeof(uint32_t), "eid0 must be uint32_t-sized");
  static_assert(sizeof(decltype(std::declval<XsectT>().eid1)) == sizeof(uint32_t), "eid1 must be uint32_t-sized");
  static_assert((offsetof(XsectT, eid0) % 4u) == 0u, "eid0 must be 4-byte aligned");
  static_assert((offsetof(XsectT, eid1) % 4u) == 0u, "eid1 must be 4-byte aligned");
  static_assert((sizeof(XsectT) % 4u) == 0u, "XsectT size must be a multiple of 4 bytes");

  if (count == 0u) {
    dst_sorted_xsects_buf = VkDeviceBuf{};
    return;
  }

  if (query_map_id != 0 && query_map_id != 1) {
    throw std::runtime_error("RadixSortXsectsByQueryEid: query_map_id must be 0 or 1");
  }

  constexpr uint32_t kRadix = 256u;
  constexpr uint32_t kPassCount = 4u;
  constexpr uint32_t kWorkgroupSize = 512u;
  constexpr uint32_t kPartitionDivision = 8u;
  constexpr uint32_t kPartitionSize = kWorkgroupSize * kPartitionDivision;  // 4096

  const uint32_t partition_count = (count + kPartitionSize - 1u) / kPartitionSize;

  const uint32_t record_stride_words = static_cast<uint32_t>(sizeof(XsectT) / 4u);
  const uint32_t eid_word_offset = static_cast<uint32_t>((query_map_id == 0 ? offsetof(XsectT, eid0) : offsetof(XsectT, eid1)) / 4u);

  const std::string extract_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_extract_keys_ns.spv";
  const std::string upsweep_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_upsweep_ns.spv";
  const std::string spine_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_spine_ns.spv";
  const std::string downsweep_kv_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_downsweep_kv_ns.spv";
  const std::string gather_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_gather_ns.spv";

  // --------------------------------------------------------------------------
  // Buffers for radix keys and payload indices
  // --------------------------------------------------------------------------
  VkDeviceBuf keys_ping;
  VkDeviceBuf keys_pong;
  VkDeviceBuf indices_ping;
  VkDeviceBuf indices_pong;

  keys_ping.Init(sizeof(uint32_t) * count);
  keys_pong.Init(sizeof(uint32_t) * count);
  indices_ping.Init(sizeof(uint32_t) * count);
  indices_pong.Init(sizeof(uint32_t) * count);

  // --------------------------------------------------------------------------
  // Histogram/state buffers used by the radix kernels
  // --------------------------------------------------------------------------
  VkDeviceBuf element_count_buf;
  VkDeviceBuf global_histogram_buf;
  VkDeviceBuf partition_histogram_buf;

  element_count_buf.Init(sizeof(uint32_t));
  global_histogram_buf.Init(sizeof(uint32_t) * kRadix * kPassCount);
  partition_histogram_buf.Init(sizeof(uint32_t) * kRadix * partition_count);

  writeToStorageBuffer(element_count_buf, count);
  zeroDeviceBuffer(global_histogram_buf, sizeof(uint32_t) * kRadix * kPassCount);
  zeroDeviceBuffer(partition_histogram_buf, sizeof(uint32_t) * kRadix * partition_count);

  // --------------------------------------------------------------------------
  // Extract selected eid into keys_ping and initialize indices_ping = [0..count)
  // --------------------------------------------------------------------------
  {
    struct ExtractParams {
      uint32_t count;
      uint32_t record_stride_words;
      uint32_t eid_word_offset;
      uint32_t _pad0;
    };

    RunComputePass(count,
                   extract_spv.c_str(),
                   ExtractParams{
                       .count = count,
                       .record_stride_words = record_stride_words,
                       .eid_word_offset = eid_word_offset,
                       ._pad0 = 0u,
                   },
                   src_xsects_buf,  // binding 0
                   keys_ping,  // binding 1
                   indices_ping);  // binding 2
  }

  VkDeviceBuf* keys_src = &keys_ping;
  VkDeviceBuf* keys_dst = &keys_pong;
  VkDeviceBuf* vals_src = &indices_ping;
  VkDeviceBuf* vals_dst = &indices_pong;

  // --------------------------------------------------------------------------
  // 4 radix passes, 8 bits each
  // --------------------------------------------------------------------------
  for (uint32_t pass = 0; pass < kPassCount; ++pass) {
    zeroDeviceBuffer(global_histogram_buf, sizeof(uint32_t) * kRadix * kPassCount);
    zeroDeviceBuffer(partition_histogram_buf, sizeof(uint32_t) * kRadix * partition_count);

    struct PassParams {
      int32_t pass;
      uint32_t _pad0;
      uint32_t _pad1;
      uint32_t _pad2;
    } params{
        .pass = static_cast<int32_t>(pass),
        ._pad0 = 0u,
        ._pad1 = 0u,
        ._pad2 = 0u,
    };

    // VkComputeEngine dispatches groups = ceil(n / 64). To get exact workgroup counts
    // for shaders declared as [numthreads(512,1,1)], pass workgroupCount * 64.
    RunComputePass(partition_count * 64u,
                   upsweep_spv.c_str(),
                   params,
                   element_count_buf,  // t0
                   global_histogram_buf,  // u1
                   partition_histogram_buf,  // u2
                   *keys_src);  // t3

    RunComputePass(kRadix * 64u,
                   spine_spv.c_str(),
                   params,
                   element_count_buf,  // t0
                   global_histogram_buf,  // u1
                   partition_histogram_buf);  // u2

    RunComputePass(partition_count * 64u,
                   downsweep_kv_spv.c_str(),
                   params,
                   element_count_buf,  // t0
                   global_histogram_buf,  // u1
                   partition_histogram_buf,  // u2
                   *keys_src,  // t3
                   *keys_dst,  // u4
                   *vals_src,  // t5
                   *vals_dst);  // u6

    std::swap(keys_src, keys_dst);
    std::swap(vals_src, vals_dst);
  }

  // --------------------------------------------------------------------------
  // Gather full records by sorted original indices
  // --------------------------------------------------------------------------
  dst_sorted_xsects_buf = VkDeviceBuf{};
  dst_sorted_xsects_buf.Init(sizeof(XsectT) * count);

  {
    struct GatherParams {
      uint32_t count;
      uint32_t record_stride_words;
      uint32_t _pad0;
      uint32_t _pad1;
    };

    RunComputePass(count,
                   gather_spv.c_str(),
                   GatherParams{
                       .count = count,
                       .record_stride_words = record_stride_words,
                       ._pad0 = 0u,
                       ._pad1 = 0u,
                   },
                   src_xsects_buf,  // binding 0
                   *vals_src,  // binding 1: sorted source indices
                   dst_sorted_xsects_buf);  // binding 2
  }
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_RADIX_SORT_H
