#ifndef RAYJOIN_VK_DEBUG_READBACK_H
#define RAYJOIN_VK_DEBUG_READBACK_H

#include <iostream>
#include <vector>

#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_helpers.h"  // your AllocBuf + beginOneTime/endSubmitWait

template <typename T>
std::vector<T> readBackBuffer(const AllocBuf& deviceBuf,
                              uint32_t elementCount) {
  auto& vk_ctx = GetVkComputeContext();
  VkDeviceSize size = sizeof(T) * elementCount;

  // staging buffer (GPU -> CPU)
  AllocBuf staging =
      vmaCreateBufferSimple(vk_ctx.vma, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VMA_MEMORY_USAGE_CPU_ONLY);

  VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

  // copy device -> staging
  VkBufferCopy cpy{0, 0, size};
  vkCmdCopyBuffer(cmd, deviceBuf.buf, staging.buf, 1, &cpy);

  endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);

  // map and read
  std::vector<T> out(elementCount);
  void* mapped = nullptr;
  VK_CHECK(vmaMapMemory(vk_ctx.vma, staging.alloc, &mapped));
  std::memcpy(out.data(), mapped, size);
  vmaUnmapMemory(vk_ctx.vma, staging.alloc);

  vmaDestroyBuffer(vk_ctx.vma, staging.buf, staging.alloc);

  return out;
}

template <typename T>
void writeToBuffer(const AllocBuf& deviceBuf, const std::vector<T>& data) {
  auto& vk_ctx = GetVkComputeContext();
  VkDeviceSize size = sizeof(T) * data.size();

  // staging buffer (CPU -> GPU)
  AllocBuf staging =
      vmaCreateBufferSimple(vk_ctx.vma, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VMA_MEMORY_USAGE_CPU_ONLY);

  // map and write
  void* mapped = nullptr;
  VK_CHECK(vmaMapMemory(vk_ctx.vma, staging.alloc, &mapped));
  std::memcpy(mapped, data.data(), size);
  vmaUnmapMemory(vk_ctx.vma, staging.alloc);

  VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

  // copy staging -> device
  VkBufferCopy cpy{0, 0, size};
  vkCmdCopyBuffer(cmd, staging.buf, deviceBuf.buf, 1, &cpy);

  endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);

  vmaDestroyBuffer(vk_ctx.vma, staging.buf, staging.alloc);
}

#endif  // RAYJOIN_VK_DEBUG_READBACK_H
