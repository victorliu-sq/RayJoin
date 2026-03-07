#ifndef RAYJOIN_RT_ENGINE_H
#define RAYJOIN_RT_ENGINE_H

#include <vulkan/vulkan.h>

#include <vector>

#include "vk/core/vk_global_context.h"
#include "vk/engine/vk_buffer.h"

namespace rayjoin {
namespace vk {

class RTEngine {
public:
  RTEngine();
  ~RTEngine();

  // Must be called once
  void Init();

  // Equivalent to OptiX BuildAccelCustom
  VkAccelerationStructureKHR BuildAccelCustom(const VkDeviceBuf &aabb_buf, uint32_t primitive_count);

  void SetLSIQuery(VkAccelerationStructureKHR handle,
                   const VkDeviceBuf &eid_range_buf,
                   const VkDeviceBuf &base_points_buf,
                   const VkDeviceBuf &base_edges_buf,
                   const VkDeviceBuf &query_points_buf,
                   const VkDeviceBuf &query_edges_buf,
                   const VkDeviceBuf &xsect_buf,
                   const VkDeviceBuf &prof_counter_buf,
                   uint32_t xsect_capacity,
                   int query_map_id,
                   uint32_t query_edge_count);

  void RunLSI();

private:
  struct AccelEntry {
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
    VkDeviceBuf buffer;
  };

  std::vector<AccelEntry> accels_;

  const VkComputeContext *ctx_ = nullptr;
  VkDevice device_ = VK_NULL_HANDLE;

  //////////////////////////////////////////////////////
  // Vulkan RT function pointers
  //////////////////////////////////////////////////////

  PFN_vkCreateAccelerationStructureKHR fpCreateAccelerationStructureKHR = nullptr;

  PFN_vkDestroyAccelerationStructureKHR fpDestroyAccelerationStructureKHR = nullptr;

  PFN_vkGetAccelerationStructureBuildSizesKHR fpGetAccelerationStructureBuildSizesKHR = nullptr;

  PFN_vkCmdBuildAccelerationStructuresKHR fpCmdBuildAccelerationStructuresKHR = nullptr;

  PFN_vkCmdWriteAccelerationStructuresPropertiesKHR fpCmdWriteAccelerationStructuresPropertiesKHR = nullptr;

  PFN_vkCmdCopyAccelerationStructureKHR fpCmdCopyAccelerationStructureKHR = nullptr;
};

} // namespace vk
} // namespace rayjoin

#endif
