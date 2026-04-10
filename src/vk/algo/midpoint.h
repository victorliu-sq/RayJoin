#ifndef RAYJOIN_MIDPOINT_H
#define RAYJOIN_MIDPOINT_H

#include "shader/config.h"
#include "sort.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/context_ns.h"
#include "vk/util/type_native.h"
#include "vk/util/type_traits.h"

namespace rayjoin::vk {
namespace algo {

template<ContextNSType CONTEXT_T>
inline uint32_t ReorderXsectsAndComputeMidPoints(const VkDeviceBuf& xsects_sorted_by_eid_buf,
                                                 const VkDeviceBuf& xsect_index_buf,
                                                 const VkDeviceBuf& query_edges_buf,
                                                 const VkDeviceBuf& query_points_buf,
                                                 int32_t query_map_id,
                                                 uint32_t xsect_count,
                                                 uint32_t unique_count,
                                                 VkDeviceBuf& reordered_xsects_buf,
                                                 VkDeviceBuf& mid_points_buf) {
  using xsect_t = typename CONTEXT_T::xsect_t;
  using point_t = typename CONTEXT_T::point_t;

  const uint32_t n_mid_points = (xsect_count >= unique_count) ? (xsect_count - unique_count) : 0u;

  reordered_xsects_buf.Init(sizeof(xsect_t) * std::max<uint32_t>(1u, xsect_count));

  if (xsect_count == 0u) {
    mid_points_buf.Init(sizeof(point_t));
    return 0u;
  }

  copyDeviceBuffer(xsects_sorted_by_eid_buf, reordered_xsects_buf, sizeof(xsect_t) * xsect_count);

  if (n_mid_points == 0u) {
    mid_points_buf.Init(sizeof(point_t));
    return 0u;
  }

  // --------------------------------------------------------------------------
  // 1) Per-group in-place quicksort on device
  {
    struct LaunchParamsSortGroups {
      int32_t query_map_id;
      uint32_t unique_count;
      uint32_t _pad0;
      uint32_t _pad1;
    };

    std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_midpoint_group_quicksort_ns.spv";

    RunComputePass(unique_count,
                   spv.c_str(),
                   LaunchParamsSortGroups{.query_map_id = query_map_id, .unique_count = unique_count, ._pad0 = 0u, ._pad1 = 0u},
                   reordered_xsects_buf,  // binding 0 -> gXsects
                   xsect_index_buf,  // binding 1 -> gXsectIndex
                   query_edges_buf,  // binding 2 -> gQueryEdges
                   query_points_buf);  // binding 3 -> gQueryPoints
  }

  // --------------------------------------------------------------------------
  // 2) Generate midpoints per group on device
  mid_points_buf.Init(sizeof(point_t) * n_mid_points);

  {
    struct LaunchParamsGenerate {
      uint32_t unique_count;
      uint32_t _pad0;
      uint32_t _pad1;
      uint32_t _pad2;
    };

    std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_midpoint_generate_per_group_ns.spv";

    RunComputePass(unique_count,
                   spv.c_str(),
                   LaunchParamsGenerate{.unique_count = unique_count, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                   reordered_xsects_buf,  // binding 0 -> gXsects
                   xsect_index_buf,  // binding 1 -> gXsectIndex
                   mid_points_buf);  // binding 2 -> gMidPoints
  }

  return n_mid_points;
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_MIDPOINT_H
