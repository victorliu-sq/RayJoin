#ifndef RAYJOIN_VK_SORT_H
#define RAYJOIN_VK_SORT_H
#include <chrono>
#include <cstdint>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_compute_engine.h"
#include "vk/util/type_native.h"

namespace rayjoin::vk {
namespace algo {
// =================================================================================
// Sorting Xsects by Eids

inline uint32_t NextPow2U32(uint32_t v) {
  if (v <= 1u) return 1u;
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1u;
}

template<IntersectionNSType XsectT>
inline void SortXsectsByQueryEid(const VkDeviceBuf& src_xsects_buf, int32_t query_map_id, uint32_t count, VkDeviceBuf& dst_sorted_xsects_buf) {
  using Clock = std::chrono::high_resolution_clock;

  if (count == 0u) {
    dst_sorted_xsects_buf = VkDeviceBuf{};
    LOG(INFO) << "SortXsectsByQueryEid(): count=0, early return";
    return;
  }

  const auto total_t0 = Clock::now();

  const uint32_t padded_count = NextPow2U32(count);

  double init_dst_buf_ms = 0.0;
  double prepare_pass_ms = 0.0;
  double bitonic_pass_ms = 0.0;

  {
    const auto t0 = Clock::now();
    dst_sorted_xsects_buf.Init(sizeof(XsectT) * padded_count);
    const auto t1 = Clock::now();
    init_dst_buf_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  }

  {
    struct LaunchParamsPrepare {
      int32_t query_map_id;
      uint32_t xsect_count;
      uint32_t padded_count;
      uint32_t _pad0;
    };

    const std::string prepare_spv = std::string(SHADER_KERNEL_NS_DIR) + "/cop_prepare_sort_ns.spv";

    const auto t0 = Clock::now();

    RunComputePass(padded_count,
                   prepare_spv.c_str(),
                   LaunchParamsPrepare{.query_map_id = query_map_id, .xsect_count = count, .padded_count = padded_count, ._pad0 = 0u},
                   src_xsects_buf,
                   dst_sorted_xsects_buf);

    const auto t1 = Clock::now();
    prepare_pass_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  }

  {
    struct LaunchParamsSort {
      int32_t query_map_id;
      uint32_t j;
      uint32_t k;
      uint32_t count;
    };

    const std::string sort_spv = std::string(SHADER_KERNEL_NS_DIR) + "/cop_sort_xsect_bitonic_ns.spv";

    const auto t0 = Clock::now();

    VkComputeEngine<LaunchParamsSort> pass(padded_count,
                                           sort_spv.c_str(),
                                           LaunchParamsSort{.query_map_id = query_map_id, .j = 0u, .k = 0u, .count = padded_count},
                                           dst_sorted_xsects_buf);

    for (uint32_t k = 2u; k <= padded_count; k <<= 1u) {
      for (uint32_t j = k >> 1u; j > 0u; j >>= 1u) {
        pass.setParams(LaunchParamsSort{.query_map_id = query_map_id, .j = j, .k = k, .count = padded_count});
        pass.run();
      }
    }

    const auto t1 = Clock::now();
    bitonic_pass_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  }

  const auto total_t1 = Clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(total_t1 - total_t0).count();

  LOG(INFO) << "SortXsectsByQueryEid(): query_map_id=" << query_map_id << " count=" << count << " padded_count=" << padded_count;
  LOG(INFO) << "  Init dst_sorted_xsects_buf: " << init_dst_buf_ms << " ms";
  LOG(INFO) << "  Prepare xsects compute pass: " << prepare_pass_ms << " ms";
  LOG(INFO) << "  Bitonic sort passes (direct xsect sort): " << bitonic_pass_ms << " ms";
  LOG(INFO) << "  Total: " << total_ms << " ms";
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_VK_SORT_H
