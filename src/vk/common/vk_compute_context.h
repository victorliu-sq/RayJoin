#ifndef RAYJOIN_VK_CONTEXT_H
#define RAYJOIN_VK_CONTEXT_H

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

struct VkComputeContext {
  VkPhysicalDevice phys = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;

  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;

  VkCommandPool cmdPool = VK_NULL_HANDLE;

  VmaAllocator vma = VK_NULL_HANDLE;

  // You can pass an existing pool, or we can create one
  VkDescriptorPool descPool = VK_NULL_HANDLE;
};


#endif  // RAYJOIN_VK_CONTEXT_H
