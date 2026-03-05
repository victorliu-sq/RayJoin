#ifndef RAYJOIN_VKBUFFERMGR_H
#define RAYJOIN_VKBUFFERMGR_H

#include <vk_mem_alloc.h>

#include "vk_compute_context.h"

namespace rayjoin {
namespace vk {
class VkBufferMgr {
 public:
  explicit VkBufferMgr(const VkComputeContext& ctx) {
    // VMA needs instance to load function pointers
    VmaAllocatorCreateInfo vaci{};
    vaci.instance = ctx.instance;
    vaci.physicalDevice = ctx.phys;
    vaci.device = ctx.device;
    vaci.vulkanApiVersion = VK_API_VERSION_1_3;
    VK_CHECK(vmaCreateAllocator(&vaci, &allocator_));
  }

  ~VkBufferMgr() {
    if (!allocator_) {
      vmaDestroyAllocator(allocator_);
    }
  }

 private:
  VmaAllocator allocator_ = VK_NULL_HANDLE;
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_VKBUFFERMGR_H
