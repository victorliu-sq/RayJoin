#ifndef RAYJOIN_02_DEDUP_H
#define RAYJOIN_02_DEDUP_H

#include <cstdint>

#include "vk/algo/exclusive_scan.h"
#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_engine.h"
#include "vk/util/type_native.h"

namespace rayjoin::vk {
namespace algo {

// =================================================================================
// Deduplicate sorted xsects into sorted unique eids

inline void DedupSortedXsectsToUniqueEids(
    const VkDeviceBuf& sorted_xsects_buf, int32_t query_map_id, uint32_t xsect_count, VkDeviceBuf& unique_eids_buf, VkDeviceBuf& unique_count_buf) {
  unique_eids_buf.Init(sizeof(index_t) * std::max<uint32_t>(1u, xsect_count));
  unique_count_buf.Init(sizeof(uint32_t));

  writeToStorageBuffer<uint32_t>(unique_count_buf, 0u);

  if (xsect_count == 0u) {
    return;
  }

  VkDeviceBuf is_unique_buf;
  is_unique_buf.Init(sizeof(uint32_t) * xsect_count);

  VkDeviceBuf unique_positions_buf;
  unique_positions_buf.Init(sizeof(uint32_t) * xsect_count);

  // --------------------------------------------------------------------------
  // 1) Mark first occurrence of each unique eid in sorted xsects
  {
    struct LaunchParamsMarkUnique {
      int32_t query_map_id;
      uint32_t xsect_count;
      uint32_t _pad0;
      uint32_t _pad1;
    };

    std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_unique_mark_eids_ns.spv";

    RunComputePass(xsect_count,
                   spv.c_str(),
                   LaunchParamsMarkUnique{.query_map_id = query_map_id, .xsect_count = xsect_count, ._pad0 = 0u, ._pad1 = 0u},
                   sorted_xsects_buf,  // binding 0 -> gXsectsSorted
                   is_unique_buf);  // binding 1 -> gIsUnique
  }

  // --------------------------------------------------------------------------
  // 2) Exclusive scan: is_unique -> unique_positions
  ExclusiveScanUInt32(is_unique_buf, unique_positions_buf, xsect_count);

  // --------------------------------------------------------------------------
  // 3) Scatter unique eids to stable positions
  {
    struct LaunchParamsScatterUnique {
      int32_t query_map_id;
      uint32_t xsect_count;
      uint32_t _pad0;
      uint32_t _pad1;
    };

    std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_unique_scatter_eids_ns.spv";

    RunComputePass(xsect_count,
                   spv.c_str(),
                   LaunchParamsScatterUnique{.query_map_id = query_map_id, .xsect_count = xsect_count, ._pad0 = 0u, ._pad1 = 0u},
                   sorted_xsects_buf,  // binding 0 -> gXsectsSorted
                   is_unique_buf,  // binding 1 -> gIsUnique
                   unique_positions_buf,  // binding 2 -> gUniquePositions
                   unique_eids_buf);  // binding 3 -> gUniqueEids
  }

  // --------------------------------------------------------------------------
  // 4) Finalize unique_count = last_position + last_flag
  {
    struct LaunchParamsFinalizeUniqueCount {
      uint32_t xsect_count;
      uint32_t _pad0;
      uint32_t _pad1;
      uint32_t _pad2;
    };

    std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_unique_finalize_count_ns.spv";

    RunComputePass(1u,
                   spv.c_str(),
                   LaunchParamsFinalizeUniqueCount{.xsect_count = xsect_count, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                   is_unique_buf,  // binding 0 -> gIsUnique
                   unique_positions_buf,  // binding 1 -> gUniquePositions
                   unique_count_buf);  // binding 2 -> gUniqueCount
  }
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_02_DEDUP_H
