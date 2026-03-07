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
  VkAccelerationStructureKHR BuildAccelCustom(const VkDeviceBuf& aabb_buf,
                                              uint32_t primitive_count);

 private:
  struct AccelEntry {
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
    VkDeviceBuf buffer;
  };

  std::vector<AccelEntry> accels_;

  const VkComputeContext* ctx_ = nullptr;
  VkDevice device_ = VK_NULL_HANDLE;

  //////////////////////////////////////////////////////
  // Vulkan RT function pointers
  //////////////////////////////////////////////////////

  PFN_vkCreateAccelerationStructureKHR fpCreateAccelerationStructureKHR = nullptr;

  PFN_vkDestroyAccelerationStructureKHR fpDestroyAccelerationStructureKHR = nullptr;

  PFN_vkGetAccelerationStructureBuildSizesKHR fpGetAccelerationStructureBuildSizesKHR = nullptr;

  PFN_vkCmdBuildAccelerationStructuresKHR fpCmdBuildAccelerationStructuresKHR = nullptr;

  PFN_vkCmdWriteAccelerationStructuresPropertiesKHR
      fpCmdWriteAccelerationStructuresPropertiesKHR = nullptr;

  PFN_vkCmdCopyAccelerationStructureKHR
      fpCmdCopyAccelerationStructureKHR = nullptr;
};

}  // namespace vk
}  // namespace rayjoin

#endif