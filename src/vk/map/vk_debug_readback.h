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
  VkDeviceSize size = sizeof(T) * elementCount;
  std::vector<T> out(elementCount);
  VkStagingBuf staging(size);
  staging.Device2Stage(deviceBuf, size);
  staging.Stage2Host(out);
  return out;
}

template <typename T>
void writeToStorageBuffer(const AllocBuf& deviceBuf, const std::vector<T>& in) {
  auto& vk_ctx = GetVkComputeContext();
  uint32_t elementCount = static_cast<uint32_t>(in.size());
  VkDeviceSize size = sizeof(T) * elementCount;
  VkStagingBuf staging(size);
  staging.Host2Stage(in);
  staging.Stage2Device(deviceBuf, size);
}

#endif  // RAYJOIN_VK_DEBUG_READBACK_H
