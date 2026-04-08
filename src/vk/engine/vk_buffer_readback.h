#ifndef RAYJOIN_VK_DEBUG_READBACK_H
#define RAYJOIN_VK_DEBUG_READBACK_H

#include <vector>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_helpers.h"  // your AllocBuf + beginOneTime/endSubmitWait
#include "vk_compute_engine.h"

// template <typename T>
// std::vector<T> readBackStorageBuffer(const AllocBuf& deviceBuf,
//                               uint32_t elementCount) {
//   VkDeviceSize size = sizeof(T) * elementCount;
//   std::vector<T> out(elementCount);
//   VkStagingBuf staging(size);
//   staging.Device2Stage(deviceBuf, size);
//   staging.Stage2Host(out);
//   return out;
// }
//
// template <typename T>
// void writeToStorageBuffer(const AllocBuf& deviceBuf, const std::vector<T>&
// in) {
//   auto& vk_ctx = GetVkComputeContext();
//   uint32_t elementCount = static_cast<uint32_t>(in.size());
//   VkDeviceSize size = sizeof(T) * elementCount;
//   VkStagingBuf staging(size);
//   staging.Host2Stage(in);
//   staging.Stage2Device(deviceBuf, size);
// }

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

// =================================================================================
// Sorting
inline uint32_t NextPow2U32(uint32_t v) {
  if (v <= 1u) return 1u;
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1u;
}

inline void SortXsectByQueryEid(const VkDeviceBuf& sortEntriesBuf, const VkDeviceBuf& xsectsBuf, uint32_t count) {
  if (count <= 1u) {
    return;
  }

  const uint32_t padded_count = NextPow2U32(count);
  std::string sort_spv = std::string(SHADER_KERNEL_NS_DIR) + "/cop_sort_xsect_bitonic_ns.spv";

  struct LaunchParamsSort {
    uint32_t j;
    uint32_t k;
    uint32_t count;
    uint32_t _pad0;
  };

  for (uint32_t k = 2u; k <= padded_count; k <<= 1u) {
    for (uint32_t j = k >> 1u; j > 0u; j >>= 1u) {
      rayjoin::vk::RunComputePass(padded_count,
                                  sort_spv.c_str(),
                                  LaunchParamsSort{.j = j, .k = k, .count = padded_count, ._pad0 = 0u},
                                  sortEntriesBuf,  // binding 0 -> gSortEntries
                                  xsectsBuf);  // binding 1 -> gXsects
    }
  }
}

#endif  // RAYJOIN_VK_DEBUG_READBACK_H
