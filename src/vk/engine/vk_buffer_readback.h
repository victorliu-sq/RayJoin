#ifndef RAYJOIN_VK_DEBUG_READBACK_H
#define RAYJOIN_VK_DEBUG_READBACK_H

#include <vector>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_helpers.h"  // your AllocBuf + beginOneTime/endSubmitWait
#include "vk_compute_engine.h"

namespace rayjoin::vk {

template<typename T>
std::vector<T> readBackStorageBuffer(const VkDeviceBuf& deviceBuf, uint32_t elementCount) {
  VkDeviceSize size = sizeof(T) * elementCount;
  std::vector<T> out(elementCount);
  VkStagingBuf staging(size);
  staging.Device2Stage(deviceBuf, size);
  staging.Stage2Host(out);
  return out;
}

template<typename T>
T readBackStorageBuffer(const VkDeviceBuf& deviceBuf) {
  auto tmp = readBackStorageBuffer<T>(deviceBuf, 1);
  return tmp[0];
}

template<typename T>
void writeToStorageBuffer(const VkDeviceBuf& deviceBuf, const std::vector<T>& in) {
  auto& vk_ctx = GetVkComputeContext();
  uint32_t elementCount = static_cast<uint32_t>(in.size());
  VkDeviceSize size = sizeof(T) * elementCount;
  VkStagingBuf staging(size);
  staging.Host2Stage(in);
  staging.Stage2Device(deviceBuf, size);
}

template<typename T>
void writeToStorageBuffer(const VkDeviceBuf& deviceBuf, const T& obj) {
  std::vector<T> tmp{obj};
  writeToStorageBuffer(deviceBuf, tmp);
}

inline void copyDeviceBuffer(const VkDeviceBuf& src, const VkDeviceBuf& dst, VkDeviceSize size) {
  auto& vk_ctx = GetVkComputeContext();

  VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

  VkBufferCopy cpy{0, 0, size};
  vkCmdCopyBuffer(cmd, src.Buf(), dst.Buf(), 1, &cpy);

  endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
}

inline void zeroDeviceBuffer(const VkDeviceBuf& buf, VkDeviceSize size) {
  auto& vk_ctx = GetVkComputeContext();
  VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);
  vkCmdFillBuffer(cmd, buf.Buf(), 0, size, 0u);
  endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
}

}  // namespace rayjoin::vk


#endif  // RAYJOIN_VK_DEBUG_READBACK_H
