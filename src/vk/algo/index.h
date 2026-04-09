#ifndef RAYJOIN_INDEX_H
#define RAYJOIN_INDEX_H

#include <cstdint>

#include "vk/algo/exclusive_scan.h"
#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_engine.h"
#include "vk/util/type_native.h"

namespace rayjoin::vk {
namespace algo {

// =================================================================================
// Build xsect_index for each unique eid from sorted xsects
//
// Inputs:
//   sorted_xsects_buf : xsects already sorted by query eid
//   unique_eids_buf   : sorted unique eids corresponding to those xsects
//   query_map_id      : 0 => use eid0, 1 => use eid1
//   xsect_count       : total # of sorted xsects
//   unique_count      : total # of unique eids
//
// Output:
//   xsect_index_buf   : length = unique_count + 1
//                       xsect_index[0] = 0
//                       xsect_index[i+1] = prefix sum of counts through i
//
inline void BuildXsectIndexFromSortedXsects(const VkDeviceBuf& sorted_xsects_buf,
                                            const VkDeviceBuf& unique_eids_buf,
                                            int32_t query_map_id,
                                            uint32_t xsect_count,
                                            uint32_t unique_count,
                                            VkDeviceBuf& xsect_index_buf) {
  xsect_index_buf.Init(sizeof(uint32_t) * std::max<uint32_t>(1u, unique_count + 1u));

  if (unique_count == 0u) {
    writeToStorageBuffer<uint32_t>(xsect_index_buf, std::vector<uint32_t>{0u});
    return;
  }

  VkDeviceBuf n_xsects_per_edge_buf;
  n_xsects_per_edge_buf.Init(sizeof(uint32_t) * unique_count);

  VkDeviceBuf unique_positions_buf;
  unique_positions_buf.Init(sizeof(uint32_t) * unique_count);

  // --------------------------------------------------------------------------
  // 1) Count occurrences of each unique eid in sorted xsects
  {
    struct LaunchParamsCountPerEid {
      int32_t query_map_id;
      uint32_t xsect_count;
      uint32_t unique_count;
      uint32_t _pad0;
    };

    std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_index_count_per_eid_ns.spv";

    RunComputePass(unique_count,
                   spv.c_str(),
                   LaunchParamsCountPerEid{.query_map_id = query_map_id, .xsect_count = xsect_count, .unique_count = unique_count, ._pad0 = 0u},
                   sorted_xsects_buf,  // binding 0 -> gXsectsSorted
                   unique_eids_buf,  // binding 1 -> gUniqueEids
                   n_xsects_per_edge_buf  // binding 2 -> gNXsectsPerEdge
    );
  }

  // --------------------------------------------------------------------------
  // 2) Exclusive scan counts -> starting positions
  ExclusiveScanUInt32(n_xsects_per_edge_buf, unique_positions_buf, unique_count);

  // --------------------------------------------------------------------------
  // 3) Finalize xsect_index:
  //    xsect_index[0] = 0
  //    xsect_index[i+1] = unique_positions[i] + n_xsects_per_edge[i]
  {
    struct LaunchParamsFinalizeIndex {
      uint32_t unique_count;
      uint32_t _pad0;
      uint32_t _pad1;
      uint32_t _pad2;
    };

    std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_index_finalize_ns.spv";

    RunComputePass(unique_count,
                   spv.c_str(),
                   LaunchParamsFinalizeIndex{.unique_count = unique_count, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                   n_xsects_per_edge_buf,  // binding 0 -> gNXsectsPerEdge
                   unique_positions_buf,  // binding 1 -> gUniquePositions
                   xsect_index_buf  // binding 2 -> gXsectIndex
    );
  }
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_INDEX_H
