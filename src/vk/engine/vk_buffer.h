#ifndef RAYJOIN_VK_BUFFER_H
#define RAYJOIN_VK_BUFFER_H
#include "vk/core/vk_global_context.h"
#include "vk_compute_context.h"
#include "vk_helpers.h"
#include "vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"

class VkAbsBuf {
 public:
  VkAbsBuf() : vk_ctx(GetVkComputeContext()) {};

  virtual ~VkAbsBuf() { vmaDestroyBuffer(vk_ctx.vma, buf, alloc); }

  VkBuffer Buf() const { return buf; }
  VmaAllocation Alloc() const { return alloc; }

  VkDeviceAddress DeviceAddress() const {
    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buf;

    return vkGetBufferDeviceAddress(vk_ctx.device, &info);
  }

  // ❌ disable copying
  VkAbsBuf(const VkAbsBuf&) = delete;
  VkAbsBuf& operator=(const VkAbsBuf&) = delete;

  // ✅ enable moving
  VkAbsBuf(VkAbsBuf&& other) noexcept : vk_ctx(other.vk_ctx), buf(other.buf), alloc(other.alloc) {
    other.buf = VK_NULL_HANDLE;
    other.alloc = VK_NULL_HANDLE;
  }

  VkAbsBuf& operator=(VkAbsBuf&& other) noexcept {
    if (this != &other) {
      buf = other.buf;
      alloc = other.alloc;

      other.buf = VK_NULL_HANDLE;
      other.alloc = VK_NULL_HANDLE;
    }
    return *this;
  }

 protected:
  // VmaAllocator vma = VK_NULL_HANDLE;
  const VkComputeContext& vk_ctx;

  VkBuffer buf = VK_NULL_HANDLE;
  VmaAllocation alloc = VK_NULL_HANDLE;

  // ==================================================================
  // Helper method
  void createBufferSimple(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ai{};
    ai.usage = memUsage;

    VK_CHECK(vmaCreateBuffer(vk_ctx.vma, &bi, &ai, &buf, &alloc, nullptr));
  }
};

class VkDeviceBuf : public VkAbsBuf {
 public:
  VkDeviceBuf() = default;
  // VkDeviceBuf(VkDeviceSize size) {
  //   createBufferSimple(size,
  //                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
  //                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
  //                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  //                      VMA_MEMORY_USAGE_GPU_ONLY);
  // }

  void Init(VkDeviceSize size) {
    createBufferSimple(size,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           // REQUIRED FOR RAY TRACING
                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                           VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                       VMA_MEMORY_USAGE_GPU_ONLY);
  }

  void InitAS(VkDeviceSize size) {
    createBufferSimple(
        size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
  }

  void InitSBT(VkDeviceSize size) {
    createBufferSimple(size,
                       VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VMA_MEMORY_USAGE_GPU_ONLY);
  }
};

class VkStagingBuf : public VkAbsBuf {
 public:
  VkStagingBuf(VkDeviceSize size) {
    createBufferSimple(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
  }

  template<typename T>
  void Stage2Host(std::vector<T>& out_vec) {
    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(vk_ctx.vma, alloc, &mapped));
    std::memcpy(out_vec.data(), mapped, sizeof(T) * out_vec.size());
    vmaUnmapMemory(vk_ctx.vma, alloc);
  }

  template<typename T>
  void Host2Stage(const std::vector<T>& in_vec) {
    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(vk_ctx.vma, alloc, &mapped));
    std::memcpy(mapped, in_vec.data(), sizeof(T) * in_vec.size());
    vmaUnmapMemory(vk_ctx.vma, alloc);
  }

  void Device2Stage(const AllocBuf& deviceBuf, VkDeviceSize size) {
    VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

    VkBufferCopy cpy{0, 0, size};
    vkCmdCopyBuffer(cmd, deviceBuf.buf, buf, 1, &cpy);

    endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
  }

  void Stage2Device(const AllocBuf& deviceBuf, VkDeviceSize size) {
    VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

    VkBufferCopy cpy{0, 0, size};
    vkCmdCopyBuffer(cmd, buf, deviceBuf.buf, 1, &cpy);

    endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
  }

  void Device2Stage(const VkDeviceBuf& deviceBuf, VkDeviceSize size) {
    VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

    VkBufferCopy cpy{0, 0, size};
    vkCmdCopyBuffer(cmd, deviceBuf.Buf(), buf, 1, &cpy);

    endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
  }

  void Stage2Device(const VkDeviceBuf& deviceBuf, VkDeviceSize size) {
    VkCommandBuffer cmd = beginOneTime(vk_ctx.device, vk_ctx.cmdPool);

    VkBufferCopy cpy{0, 0, size};
    vkCmdCopyBuffer(cmd, buf, deviceBuf.Buf(), 1, &cpy);

    endSubmitWait(vk_ctx.device, vk_ctx.queue, vk_ctx.cmdPool, cmd);
  }
};

#endif  // RAYJOIN_VK_BUFFER_H
