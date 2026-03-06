#ifndef RAYJOIN_VK_BUFFER_H
#define RAYJOIN_VK_BUFFER_H
#include "vk_helpers.h"
#include "vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"

namespace rayjoin {
namespace vk {
class VkAbsBuf {
 public:
  VkAbsBuf() = default;

  virtual ~VkAbsBuf() {
    if (buf && vma) {
      vmaDestroyBuffer(vma, buf, alloc);
    }
  }

  VkBuffer getBuf() const { return buf; }
  VmaAllocation getAlloc() const { return alloc; }

 protected:
  VmaAllocator vma = VK_NULL_HANDLE;
  VkBuffer buf = VK_NULL_HANDLE;
  VmaAllocation alloc = VK_NULL_HANDLE;

  void createBufferSimple(VmaAllocator allocator, VkDeviceSize size,
                          VkBufferUsageFlags usage, VmaMemoryUsage memUsage) {
    vma = allocator;

    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = memUsage;

    VK_CHECK(vmaCreateBuffer(vma, &bi, &ai, &buf, &alloc, nullptr));
  }
};

class VkStorageBuf : public VkAbsBuf {
 public:
  VkStorageBuf(VmaAllocator vma, VkDeviceSize size) {
    createBufferSimple(vma, size,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VMA_MEMORY_USAGE_GPU_ONLY);
  }
};

class VkStagingBuf : public VkAbsBuf {
 public:
  VkStagingBuf(VmaAllocator vma, VkDeviceSize size) {
    createBufferSimple(
        vma, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);
  }
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_VK_BUFFER_H
