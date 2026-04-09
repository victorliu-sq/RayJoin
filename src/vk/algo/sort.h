#ifndef RAYJOIN_VK_SORT_H
#define RAYJOIN_VK_SORT_H
#include <cstdint>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_compute_engine.h"

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

struct XsectSortEntryGPU {
  index_t query_eid;
  uint32_t src_idx;
  uint32_t _pad0;
  uint32_t _pad1;
};
static_assert(std::is_trivially_copyable_v<XsectSortEntryGPU>);

template<typename XsectT>
inline void SortXsectsByQueryEid(const VkDeviceBuf& src_xsects_buf, int32_t query_map_id, uint32_t count, VkDeviceBuf& dst_sorted_xsects_buf) {
  if (count == 0u) {
    dst_sorted_xsects_buf = VkDeviceBuf{};
    return;
  }

  const uint32_t padded_count = NextPow2U32(count);

  dst_sorted_xsects_buf.Init(sizeof(XsectT) * padded_count);

  VkDeviceBuf sort_entries_buf;
  sort_entries_buf.Init(sizeof(XsectSortEntryGPU) * padded_count);

  {
    struct LaunchParamsPrepareSortEntries {
      int32_t query_map_id;
      uint32_t xsect_count;
      uint32_t padded_count;
      uint32_t _pad0;
    };

    std::string prepare_spv = std::string(SHADER_KERNEL_NS_DIR) + "/cop_prepare_sort_entries_ns.spv";

    RunComputePass(padded_count,
                   prepare_spv.c_str(),
                   LaunchParamsPrepareSortEntries{.query_map_id = query_map_id, .xsect_count = count, .padded_count = padded_count, ._pad0 = 0u},
                   src_xsects_buf,  // binding 0 -> gXsectsIn
                   dst_sorted_xsects_buf,  // binding 1 -> gXsectsOut
                   sort_entries_buf);  // binding 2 -> gSortEntries
  }

  {
    std::string sort_spv = std::string(SHADER_KERNEL_NS_DIR) + "/cop_sort_xsect_bitonic_ns.spv";

    struct LaunchParamsSort {
      uint32_t j;
      uint32_t k;
      uint32_t count;
      uint32_t _pad0;
    };

    for (uint32_t k = 2u; k <= padded_count; k <<= 1u) {
      for (uint32_t j = k >> 1u; j > 0u; j >>= 1u) {
        RunComputePass(padded_count,
                       sort_spv.c_str(),
                       LaunchParamsSort{.j = j, .k = k, .count = padded_count, ._pad0 = 0u},
                       sort_entries_buf,  // binding 0 -> gSortEntries
                       dst_sorted_xsects_buf);  // binding 1 -> gXsects
      }
    }
  }
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_VK_SORT_H
