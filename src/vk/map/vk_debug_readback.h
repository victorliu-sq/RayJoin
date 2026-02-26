#ifndef RAYJOIN_VK_DEBUG_READBACK_H
#define RAYJOIN_VK_DEBUG_READBACK_H

#include "vk/common/vk_helpers.h"   // your AllocBuf + beginOneTime/endSubmitWait
#include "vk/common/vk_context.h"
#include <vector>
#include <iostream>

template <typename T>
std::vector<T> readBackBuffer(
    const VkComputeContext& ctx,
    const AllocBuf& deviceBuf,
    uint32_t elementCount)
{
  VkDeviceSize size = sizeof(T) * elementCount;

  // staging buffer (GPU -> CPU)
  AllocBuf staging = vmaCreateBufferSimple(
      ctx.vma,
      size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY);

  VkCommandBuffer cmd = beginOneTime(ctx.device, ctx.cmdPool);

  // copy device -> staging
  VkBufferCopy cpy{0, 0, size};
  vkCmdCopyBuffer(cmd, deviceBuf.buf, staging.buf, 1, &cpy);

  endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);

  // map and read
  std::vector<T> out(elementCount);
  void* mapped = nullptr;
  VK_CHECK(vmaMapMemory(ctx.vma, staging.alloc, &mapped));
  std::memcpy(out.data(), mapped, size);
  vmaUnmapMemory(ctx.vma, staging.alloc);

  vmaDestroyBuffer(ctx.vma, staging.buf, staging.alloc);

  return out;
}

#endif  // RAYJOIN_VK_DEBUG_READBACK_H
