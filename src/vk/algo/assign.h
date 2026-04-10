#ifndef RAYJOIN_ASSIGN_H
#define RAYJOIN_ASSIGN_H

#include "vk/engine/vk_buffer.h"
#include "vk/map/context_ns.h"

namespace rayjoin::vk {
namespace algo {

template<ContextNSType CONTEXT_T>
inline void AssignMidPointPolygonIdsToXsects(const VkDeviceBuf& reordered_xsects_buf,
                                             const VkDeviceBuf& xsect_index_buf,
                                             const VkDeviceBuf& mid_point_in_polygon_buf,
                                             uint32_t unique_count) {
  if (unique_count == 0u) {
    return;
  }

  struct LaunchParamsAssign {
    uint32_t unique_count;
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
  };

  std::string spv = std::string(SHADER_KERNEL_NS_DIR) + "/algo_midpoint_assign_poly_ids_ns.spv";

  RunComputePass(unique_count,
                 spv.c_str(),
                 LaunchParamsAssign{.unique_count = unique_count, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                 reordered_xsects_buf,  // binding 0 -> gXsects
                 xsect_index_buf,  // binding 1 -> gXsectIndex
                 mid_point_in_polygon_buf  // binding 2 -> gMidPolyIds
  );
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_ASSIGN_H
