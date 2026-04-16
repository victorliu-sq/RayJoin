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

// template<IntersectionNSType XsectT>
// inline void RadixSortXsectsByQueryEid(const VkDeviceBuf& src_xsects_buf, int32_t query_map_id, uint32_t count, VkDeviceBuf& dst_sorted_xsects_buf)
// {
//   if (query_map_id != 0 && query_map_id != 1) {
//     throw std::runtime_error("RadixSortXsectsByQueryEid: query_map_id must be 0 or 1");
//   }
//
//   if (count == 0u) {
//     dst_sorted_xsects_buf = VkDeviceBuf{};
//     return;
//   }
//
//   constexpr uint32_t kRadix = 256u;
//   constexpr uint32_t kPassCount = 4u;
//   constexpr uint32_t kWorkgroupSize = 512u;
//   constexpr uint32_t kPartitionDivision = 8u;
//   constexpr uint32_t kPartitionSize = kWorkgroupSize * kPartitionDivision;  // 4096
//
//   const uint32_t partition_count = (count + kPartitionSize - 1u) / kPartitionSize;
//
//   const uint32_t record_stride_words = static_cast<uint32_t>(sizeof(XsectT) / 4u);
//   const uint32_t eid_word_offset = static_cast<uint32_t>((query_map_id == 0 ? offsetof(XsectT, eid0) : offsetof(XsectT, eid1)) / 4u);
//
//   const std::string extract_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_extract_keys_ns.spv";
//   const std::string upsweep_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_upsweep_ns.spv";
//   const std::string spine_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_spine_ns.spv";
//   const std::string downsweep_kv_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_downsweep_kv_ns.spv";
//   const std::string gather_spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_radix_sort_xsects_gather_ns.spv";
//
//   // --------------------------------------------------------------------------
//   // Buffers for radix keys and payload indices
//   // --------------------------------------------------------------------------
//   VkDeviceBuf keys_ping;
//   VkDeviceBuf keys_pong;
//   VkDeviceBuf indices_ping;
//   VkDeviceBuf indices_pong;
//
//   keys_ping.Init(sizeof(uint32_t) * count);
//   keys_pong.Init(sizeof(uint32_t) * count);
//   indices_ping.Init(sizeof(uint32_t) * count);
//   indices_pong.Init(sizeof(uint32_t) * count);
//
//   // --------------------------------------------------------------------------
//   // Histogram/state buffers used by the radix kernels
//   // --------------------------------------------------------------------------
//   VkDeviceBuf element_count_buf;
//   VkDeviceBuf global_histogram_buf;
//   VkDeviceBuf partition_histogram_buf;
//
//   element_count_buf.Init(sizeof(uint32_t));
//   global_histogram_buf.Init(sizeof(uint32_t) * kRadix * kPassCount);
//   partition_histogram_buf.Init(sizeof(uint32_t) * kRadix * partition_count);
//
//   writeToStorageBuffer(element_count_buf, count);
//   zeroDeviceBuffer(global_histogram_buf, sizeof(uint32_t) * kRadix * kPassCount);
//   zeroDeviceBuffer(partition_histogram_buf, sizeof(uint32_t) * kRadix * partition_count);
//
//   // --------------------------------------------------------------------------
//   // Extract selected eid into keys_ping and initialize indices_ping = [0..count)
//   // --------------------------------------------------------------------------
//   {
//     struct ExtractParams {
//       uint32_t count;
//       uint32_t record_stride_words;
//       uint32_t eid_word_offset;
//       uint32_t _pad0;
//     };
//
//     RunComputePass(count,
//                    extract_spv.c_str(),
//                    ExtractParams{
//                        .count = count,
//                        .record_stride_words = record_stride_words,
//                        .eid_word_offset = eid_word_offset,
//                        ._pad0 = 0u,
//                    },
//                    src_xsects_buf,  // binding 0
//                    keys_ping,  // binding 1
//                    indices_ping);  // binding 2
//   }
//
//   VkDeviceBuf* keys_src = &keys_ping;
//   VkDeviceBuf* keys_dst = &keys_pong;
//   VkDeviceBuf* vals_src = &indices_ping;
//   VkDeviceBuf* vals_dst = &indices_pong;
//
//   // --------------------------------------------------------------------------
//   // 4 radix passes, 8 bits each
//   // --------------------------------------------------------------------------
//   for (uint32_t pass = 0; pass < kPassCount; ++pass) {
//     zeroDeviceBuffer(global_histogram_buf, sizeof(uint32_t) * kRadix * kPassCount);
//     zeroDeviceBuffer(partition_histogram_buf, sizeof(uint32_t) * kRadix * partition_count);
//
//     struct PassParams {
//       int32_t pass;
//       uint32_t _pad0;
//       uint32_t _pad1;
//       uint32_t _pad2;
//     } params{
//         .pass = static_cast<int32_t>(pass),
//         ._pad0 = 0u,
//         ._pad1 = 0u,
//         ._pad2 = 0u,
//     };
//
//     // VkComputeEngine dispatches groups = ceil(n / 64). To get exact workgroup counts
//     // for shaders declared as [numthreads(512,1,1)], pass workgroupCount * 64.
//     RunComputePass(partition_count * 64u,
//                    upsweep_spv.c_str(),
//                    params,
//                    element_count_buf,  // t0
//                    global_histogram_buf,  // u1
//                    partition_histogram_buf,  // u2
//                    *keys_src);  // t3
//
//     RunComputePass(kRadix * 64u,
//                    spine_spv.c_str(),
//                    params,
//                    element_count_buf,  // t0
//                    global_histogram_buf,  // u1
//                    partition_histogram_buf);  // u2
//
//     RunComputePass(partition_count * 64u,
//                    downsweep_kv_spv.c_str(),
//                    params,
//                    element_count_buf,  // t0
//                    global_histogram_buf,  // u1
//                    partition_histogram_buf,  // u2
//                    *keys_src,  // t3
//                    *keys_dst,  // u4
//                    *vals_src,  // t5
//                    *vals_dst);  // u6
//
//     std::swap(keys_src, keys_dst);
//     std::swap(vals_src, vals_dst);
//   }
//
//   // --------------------------------------------------------------------------
//   // Gather full records by sorted original indices
//   // --------------------------------------------------------------------------
//   dst_sorted_xsects_buf = VkDeviceBuf{};
//   dst_sorted_xsects_buf.Init(sizeof(XsectT) * count);
//
//   {
//     struct GatherParams {
//       uint32_t count;
//       uint32_t record_stride_words;
//       uint32_t _pad0;
//       uint32_t _pad1;
//     };
//
//     RunComputePass(count,
//                    gather_spv.c_str(),
//                    GatherParams{
//                        .count = count,
//                        .record_stride_words = record_stride_words,
//                        ._pad0 = 0u,
//                        ._pad1 = 0u,
//                    },
//                    src_xsects_buf,  // binding 0
//                    *vals_src,  // binding 1: sorted source indices
//                    dst_sorted_xsects_buf);  // binding 2
//   }
// }

template<IntersectionNSType XsectT>
inline void RadixSortXsectsByQueryEid(const VkDeviceBuf& src_xsects_buf, int32_t query_map_id, uint32_t count, VkDeviceBuf& dst_sorted_xsects_buf) {
  if (query_map_id != 0 && query_map_id != 1) {
    throw std::runtime_error("RadixSortXsectsByQueryEid: query_map_id must be 0 or 1");
  }

  if (count == 0u) {
    dst_sorted_xsects_buf = VkDeviceBuf{};
    return;
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

  VkDeviceBuf keys_ping;
  VkDeviceBuf keys_pong;
  VkDeviceBuf indices_ping;
  VkDeviceBuf indices_pong;

  keys_ping.Init(sizeof(uint32_t) * count);
  keys_pong.Init(sizeof(uint32_t) * count);
  indices_ping.Init(sizeof(uint32_t) * count);
  indices_pong.Init(sizeof(uint32_t) * count);

  VkDeviceBuf element_count_buf;
  VkDeviceBuf global_histogram_buf;
  VkDeviceBuf partition_histogram_buf;

  element_count_buf.Init(sizeof(uint32_t));
  global_histogram_buf.Init(sizeof(uint32_t) * kRadix * kPassCount);
  partition_histogram_buf.Init(sizeof(uint32_t) * kRadix * partition_count);

  writeToStorageBuffer(element_count_buf, count);

  dst_sorted_xsects_buf = VkDeviceBuf{};
  dst_sorted_xsects_buf.Init(sizeof(XsectT) * count);

  struct ExtractParams {
    uint32_t count;
    uint32_t record_stride_words;
    uint32_t eid_word_offset;
    uint32_t _pad0;
  };

  struct PassParams {
    int32_t pass;
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
  };

  struct GatherParams {
    uint32_t count;
    uint32_t record_stride_words;
    uint32_t _pad0;
    uint32_t _pad1;
  };

  // --------------------------------------------------------------------------
  // Build reusable compute engines once
  // --------------------------------------------------------------------------
  using clock = std::chrono::high_resolution_clock;
  const auto build_begin = clock::now();

  VkComputeEngine<ExtractParams> extract_pass(count,
                                              extract_spv.c_str(),
                                              ExtractParams{
                                                  .count = count,
                                                  .record_stride_words = record_stride_words,
                                                  .eid_word_offset = eid_word_offset,
                                                  ._pad0 = 0u,
                                              },
                                              src_xsects_buf,
                                              keys_ping,
                                              indices_ping);

  // upsweep for each possible source key buffer
  VkComputeEngine<PassParams> upsweep_ping_pass(partition_count * 64u,
                                                upsweep_spv.c_str(),
                                                PassParams{.pass = 0, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                                                element_count_buf,
                                                global_histogram_buf,
                                                partition_histogram_buf,
                                                keys_ping);

  VkComputeEngine<PassParams> upsweep_pong_pass(partition_count * 64u,
                                                upsweep_spv.c_str(),
                                                PassParams{.pass = 0, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                                                element_count_buf,
                                                global_histogram_buf,
                                                partition_histogram_buf,
                                                keys_pong);

  VkComputeEngine<PassParams> spine_pass(kRadix * 64u,
                                         spine_spv.c_str(),
                                         PassParams{.pass = 0, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                                         element_count_buf,
                                         global_histogram_buf,
                                         partition_histogram_buf);

  // downsweep for both ping-pong directions
  VkComputeEngine<PassParams> downsweep_ping_to_pong_pass(partition_count * 64u,
                                                          downsweep_kv_spv.c_str(),
                                                          PassParams{.pass = 0, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                                                          element_count_buf,
                                                          global_histogram_buf,
                                                          partition_histogram_buf,
                                                          keys_ping,
                                                          keys_pong,
                                                          indices_ping,
                                                          indices_pong);

  VkComputeEngine<PassParams> downsweep_pong_to_ping_pass(partition_count * 64u,
                                                          downsweep_kv_spv.c_str(),
                                                          PassParams{.pass = 0, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                                                          element_count_buf,
                                                          global_histogram_buf,
                                                          partition_histogram_buf,
                                                          keys_pong,
                                                          keys_ping,
                                                          indices_pong,
                                                          indices_ping);

  VkComputeEngine<GatherParams> gather_ping_pass(count,
                                                 gather_spv.c_str(),
                                                 GatherParams{
                                                     .count = count,
                                                     .record_stride_words = record_stride_words,
                                                     ._pad0 = 0u,
                                                     ._pad1 = 0u,
                                                 },
                                                 src_xsects_buf,
                                                 indices_ping,
                                                 dst_sorted_xsects_buf);

  VkComputeEngine<GatherParams> gather_pong_pass(count,
                                                 gather_spv.c_str(),
                                                 GatherParams{
                                                     .count = count,
                                                     .record_stride_words = record_stride_words,
                                                     ._pad0 = 0u,
                                                     ._pad1 = 0u,
                                                 },
                                                 src_xsects_buf,
                                                 indices_pong,
                                                 dst_sorted_xsects_buf);
  const auto build_end = clock::now();
  const double build_ms = std::chrono::duration_cast<std::chrono::microseconds>(build_end - build_begin).count() / 1000.0;

  LOG(INFO) << "[RadixSortXsectsByQueryEid] total VkComputeEngine construction time: " << build_ms << " ms";

  // --------------------------------------------------------------------------
  // Execute extract once
  // --------------------------------------------------------------------------
  extract_pass.run();

  // --------------------------------------------------------------------------
  // Execute 4 radix passes
  // --------------------------------------------------------------------------
  const auto rpass_begin = clock::now();
  double total_zero_global_ms = 0.0;

  const auto zero_global_begin = clock::now();
  zeroDeviceBuffer(global_histogram_buf, sizeof(uint32_t) * kRadix * kPassCount);
  // zeroDeviceBuffer(partition_histogram_buf, sizeof(uint32_t) * kRadix * partition_count);
  const auto zero_global_end = clock::now();
  total_zero_global_ms += std::chrono::duration_cast<std::chrono::microseconds>(zero_global_end - zero_global_begin).count() / 1000.0;

  // for (uint32_t pass = 0; pass < kPassCount; ++pass) {
  //   PassParams params{
  //       .pass = static_cast<int32_t>(pass),
  //       ._pad0 = 0u,
  //       ._pad1 = 0u,
  //       ._pad2 = 0u,
  //   };
  //
  //   if ((pass & 1u) == 0u) {
  //     upsweep_ping_pass.setParams(params);
  //     spine_pass.setParams(params);
  //     downsweep_ping_to_pong_pass.setParams(params);
  //
  //     upsweep_ping_pass.run();
  //     spine_pass.run();
  //     downsweep_ping_to_pong_pass.run();
  //   } else {
  //     upsweep_pong_pass.setParams(params);
  //     spine_pass.setParams(params);
  //     downsweep_pong_to_ping_pass.setParams(params);
  //
  //     upsweep_pong_pass.run();
  //     spine_pass.run();
  //     downsweep_pong_to_ping_pass.run();
  //   }
  // }
  for (uint32_t pass = 0; pass < kPassCount; ++pass) {
    PassParams params{
        .pass = static_cast<int32_t>(pass),
        ._pad0 = 0u,
        ._pad1 = 0u,
        ._pad2 = 0u,
    };

    auto& vk_ctx = GetVkComputeContext();
    VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

    // clear only current global histogram slice
    // vkCmdFillBuffer(cmd, global_histogram_buf.Buf(), sizeof(uint32_t) * kRadix * pass, sizeof(uint32_t) * kRadix, 0u);

    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};

    if ((pass & 1u) == 0u) {
      upsweep_ping_pass.setParams(params);
      spine_pass.setParams(params);
      downsweep_ping_to_pong_pass.setParams(params);

      upsweep_ping_pass.record(cmd);

      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
      vkCmdPipelineBarrier(cmd,
                           VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           0,
                           1,
                           &barrier,
                           0,
                           nullptr,
                           0,
                           nullptr);

      spine_pass.record(cmd);

      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

      downsweep_ping_to_pong_pass.record(cmd);
    } else {
      upsweep_pong_pass.setParams(params);
      spine_pass.setParams(params);
      downsweep_pong_to_ping_pass.setParams(params);

      upsweep_pong_pass.record(cmd);

      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
      vkCmdPipelineBarrier(cmd,
                           VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           0,
                           1,
                           &barrier,
                           0,
                           nullptr,
                           0,
                           nullptr);

      spine_pass.record(cmd);

      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

      downsweep_pong_to_ping_pass.record(cmd);
    }

    endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
  }

  const auto rpass_end = clock::now();
  const double rpass_ms = std::chrono::duration_cast<std::chrono::microseconds>(rpass_end - rpass_begin).count() / 1000.0;
  LOG(INFO) << "[RadixSortXsectsByQueryEid] Radix Pass time: " << rpass_ms << " ms";
  LOG(INFO) << "[RadixSortXsectsByQueryEid] Total zeroDeviceBuffer time: " << total_zero_global_ms << " ms";

  // After 4 passes, final data is back in ping buffers (even number of swaps).
  gather_ping_pass.run();
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_RADIX_SORT_H
