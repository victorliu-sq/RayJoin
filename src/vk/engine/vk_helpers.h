#ifndef RAYJOIN_VK_ALLOC_H
#define RAYJOIN_VK_ALLOC_H

#include <vulkan/vulkan.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "vk_mem_alloc.h"

#ifndef VK_CHECK
#define VK_CHECK(x)                             \
  do {                                          \
    VkResult err = (x);                         \
    if (err)                                    \
      throw std::runtime_error("Vulkan error"); \
  } while (0)
#endif

static inline void vkCheck(VkResult result) {
  if (result != VK_SUCCESS) {
    std::cerr << "Vulkan call returned an error (" << result << ")\n";
    exit(result);
  }
}

struct AllocBuf {
  VkBuffer buf = VK_NULL_HANDLE;
  VmaAllocation alloc = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  VkDeviceAddress addr = 0;
};

inline AllocBuf vmaCreateBufferSimple(VmaAllocator vma, VkDeviceSize size,
                                      VkBufferUsageFlags usage,
                                      VmaMemoryUsage memUsage) {
  AllocBuf out{};
  out.size = size;

  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = size;
  bi.usage = usage;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo ai{};
  ai.usage = memUsage;

  VK_CHECK(vmaCreateBuffer(vma, &bi, &ai, &out.buf, &out.alloc, nullptr));
  return out;
}

inline AllocBuf vmaCreateDeviceBuffer(VmaAllocator vma, VkDevice device,
                                      VkDeviceSize size,
                                      VkBufferUsageFlags usage,
                                      VmaMemoryUsage memUsage) {
  AllocBuf out{};
  out.size = size;

  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = size;
  bi.usage = usage;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo ai{};
  ai.usage = memUsage;

  VK_CHECK(vmaCreateBuffer(vma, &bi, &ai, &out.buf, &out.alloc, nullptr));

  // Get device Address
  VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
  info.buffer = out.buf;

  vkGetBufferDeviceAddress(device, &info);

  return out;
}

inline void vmaDestroyBufferSafe(VmaAllocator vma, AllocBuf& b) {
  if (b.buf) {
    vmaDestroyBuffer(vma, b.buf, b.alloc);
    b = {};
  }
}

inline std::vector<uint32_t> readSpvU32(const char* path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("Failed to open SPIR-V file");
  f.seekg(0, std::ios::end);
  size_t sz = (size_t) f.tellg();
  f.seekg(0, std::ios::beg);
  if (sz % 4)
    throw std::runtime_error("SPIR-V size not multiple of 4");
  std::vector<uint32_t> out(sz / 4);
  f.read(reinterpret_cast<char*>(out.data()), sz);
  return out;
}

inline VkCommandBuffer beginOneTime(VkDevice device, VkCommandPool pool) {
  VkCommandBufferAllocateInfo ai{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = pool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;

  VkCommandBuffer cmd{};
  VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));

  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
  return cmd;
}

inline void endSubmitWait(VkDevice device, VkQueue q, VkCommandPool pool,
                          VkCommandBuffer cmd) {
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence fence{};
  VK_CHECK(vkCreateFence(device, &fci, nullptr, &fence));

  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;

  VK_CHECK(vkQueueSubmit(q, 1, &si, fence));
  VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

  vkDestroyFence(device, fence, nullptr);
  vkFreeCommandBuffers(device, pool, 1, &cmd);
}

#endif  // RAYJOIN_VK_ALLOC_H
