#ifndef RAYJOIN_VK_DEBUG_READBACK_H
#define RAYJOIN_VK_DEBUG_READBACK_H

#include <iostream>
#include <vector>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_helpers.h"  // your AllocBuf + beginOneTime/endSubmitWait

template <typename T>
std::vector<T> readBackStorageBuffer(const AllocBuf& deviceBuf,
                              uint32_t elementCount) {
  auto& vk_ctx = GetVkComputeContext();
  VkDeviceSize size = sizeof(T) * elementCount;

  // AllocBuf staging = createStagingBuffer(vk_ctx.vma, size);
  VkStagingBuf staging(size);
  // copy device -> staging
  // VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);
  // VkBufferCopy cpy{0, 0, size};
  // vkCmdCopyBuffer(cmd, deviceBuf.buf, staging.Buf(), 1, &cpy);
  // endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
  staging.Device2Stage(deviceBuf, size);
  // map and read
  std::vector<T> out(elementCount);
  staging.Stage2Host(out);
  return out;
}

template <typename T>
void writeToStorageBuffer(const AllocBuf& deviceBuf, const std::vector<T>& in) {
  auto& vk_ctx = GetVkComputeContext();
  uint32_t elementCount = static_cast<uint32_t>(in.size());
  VkDeviceSize size = sizeof(T) * elementCount;

  // AllocBuf staging = createStagingBuffer(vk_ctx.vma, size);
  VkStagingBuf staging(size);
  // map and write
  staging.Host2Stage(in);
  // copy staging -> device
  // VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);
  // VkBufferCopy cpy{0, 0, size};
  // vkCmdCopyBuffer(cmd, staging.Buf(), deviceBuf.buf, 1, &cpy);
  // endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
  staging.Stage2Device(deviceBuf, size);
}

#endif  // RAYJOIN_VK_DEBUG_READBACK_H
