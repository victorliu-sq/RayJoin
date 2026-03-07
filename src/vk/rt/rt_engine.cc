#include "vk/rt/rt_engine.h"

#include <stdexcept>

namespace rayjoin {
  namespace vk {

    RTEngine::RTEngine() {}

    RTEngine::~RTEngine() {
      if (!ctx_ || !fpDestroyAccelerationStructureKHR)
        return;

      for (auto &e: accels_) {
        if (e.accel != VK_NULL_HANDLE) {
          fpDestroyAccelerationStructureKHR(device_, e.accel, nullptr);
        }
      }
    }

    void RTEngine::Init() {
      ctx_ = &GetVkComputeContext();
      device_ = ctx_->device;

      fpCreateAccelerationStructureKHR =
          reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR"));

      fpDestroyAccelerationStructureKHR =
          reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkDestroyAccelerationStructureKHR"));

      fpGetAccelerationStructureBuildSizesKHR =
          reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureBuildSizesKHR"));

      fpCmdBuildAccelerationStructuresKHR =
          reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR"));

      fpCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
          vkGetDeviceProcAddr(device_, "vkCmdWriteAccelerationStructuresPropertiesKHR"));

      fpCmdCopyAccelerationStructureKHR =
          reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCmdCopyAccelerationStructureKHR"));

      if (!fpCreateAccelerationStructureKHR || !fpDestroyAccelerationStructureKHR || !fpGetAccelerationStructureBuildSizesKHR ||
          !fpCmdBuildAccelerationStructuresKHR || !fpCmdWriteAccelerationStructuresPropertiesKHR || !fpCmdCopyAccelerationStructureKHR) {
        throw std::runtime_error(
            "Vulkan RT functions not loaded. "
            "Ensure device was created with:\n"
            "VK_KHR_acceleration_structure\n"
            "VK_KHR_ray_tracing_pipeline\n"
            "VK_KHR_deferred_host_operations\n"
            "VK_KHR_buffer_device_address");
      }
    }

    VkAccelerationStructureKHR RTEngine::BuildAccelCustom(const VkDeviceBuf &aabb_buf, uint32_t primitive_count) {
      auto &ctx = *ctx_;

      //////////////////////////////////////////////////////
      // Geometry
      //////////////////////////////////////////////////////
      VkAccelerationStructureGeometryKHR geometry{};
      geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
      geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
      geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

      geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
      geometry.geometry.aabbs.data.deviceAddress = aabb_buf.DeviceAddress();
      geometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

      //////////////////////////////////////////////////////
      // Range
      //////////////////////////////////////////////////////
      VkAccelerationStructureBuildRangeInfoKHR range{};
      range.primitiveCount = primitive_count;
      const VkAccelerationStructureBuildRangeInfoKHR *rangePtr = &range;

      //////////////////////////////////////////////////////
      // Build info
      //////////////////////////////////////////////////////
      VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
      buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
      buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
      buildInfo.geometryCount = 1;
      buildInfo.pGeometries = &geometry;

      //////////////////////////////////////////////////////
      // Query build sizes
      //////////////////////////////////////////////////////
      uint32_t primCounts[] = {primitive_count};

      VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
      sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

      fpGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, primCounts, &sizeInfo);

      //////////////////////////////////////////////////////
      // Allocate AS buffer
      //////////////////////////////////////////////////////
      VkDeviceBuf asBuffer;
      asBuffer.InitAS(sizeInfo.accelerationStructureSize);

      //////////////////////////////////////////////////////
      // Create AS
      //////////////////////////////////////////////////////
      VkAccelerationStructureCreateInfoKHR createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
      createInfo.buffer = asBuffer.Buf();
      createInfo.size = sizeInfo.accelerationStructureSize;
      createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

      VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
      fpCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &accel);

      //////////////////////////////////////////////////////
      // Scratch
      //////////////////////////////////////////////////////
      VkDeviceBuf scratch;
      scratch.Init(sizeInfo.buildScratchSize);

      buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      buildInfo.dstAccelerationStructure = accel;
      buildInfo.scratchData.deviceAddress = scratch.DeviceAddress();

      //////////////////////////////////////////////////////
      // Query pool
      //////////////////////////////////////////////////////
      VkQueryPool queryPool;

      VkQueryPoolCreateInfo qp{};
      qp.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
      qp.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
      qp.queryCount = 1;

      vkCreateQueryPool(device_, &qp, nullptr, &queryPool);
      vkResetQueryPool(device_, queryPool, 0, 1);

      //////////////////////////////////////////////////////
      // Build command
      //////////////////////////////////////////////////////
      VkCommandBuffer cmd = beginOneTime(ctx.device, ctx.cmdPool);

      fpCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &rangePtr);

      // Ensure build finished before querying properties
      VkMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
      barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

      vkCmdPipelineBarrier(cmd,
                           VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                           VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                           0,
                           1,
                           &barrier,
                           0,
                           nullptr,
                           0,
                           nullptr);

      fpCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &accel, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);

      endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);

      //////////////////////////////////////////////////////
      // Retrieve compact size
      //////////////////////////////////////////////////////
      VkDeviceSize compactSize = 0;

      vkGetQueryPoolResults(
          device_, queryPool, 0, 1, sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);

      vkDestroyQueryPool(device_, queryPool, nullptr);

      LOG(INFO) << "Original AS size: " << sizeInfo.accelerationStructureSize;
      LOG(INFO) << "Compacted size: " << compactSize;

      if (compactSize == 0 || compactSize > sizeInfo.accelerationStructureSize) {
        compactSize = sizeInfo.accelerationStructureSize;
      }

      //////////////////////////////////////////////////////
      // Create compact AS
      //////////////////////////////////////////////////////
      VkDeviceBuf compactBuffer;
      compactBuffer.InitAS(compactSize);

      VkAccelerationStructureCreateInfoKHR compactInfo{};
      compactInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
      compactInfo.buffer = compactBuffer.Buf();
      compactInfo.size = compactSize;
      compactInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

      VkAccelerationStructureKHR compactAccel = VK_NULL_HANDLE;

      fpCreateAccelerationStructureKHR(device_, &compactInfo, nullptr, &compactAccel);

      //////////////////////////////////////////////////////
      // Copy compact
      //////////////////////////////////////////////////////
      VkCopyAccelerationStructureInfoKHR copyInfo{};
      copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
      copyInfo.src = accel;
      copyInfo.dst = compactAccel;
      copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

      cmd = beginOneTime(ctx.device, ctx.cmdPool);

      fpCmdCopyAccelerationStructureKHR(cmd, &copyInfo);

      endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);

      //////////////////////////////////////////////////////
      // Destroy original AS
      //////////////////////////////////////////////////////
      fpDestroyAccelerationStructureKHR(device_, accel, nullptr);

      //////////////////////////////////////////////////////
      // Store
      //////////////////////////////////////////////////////
      accels_.push_back({compactAccel, std::move(compactBuffer)});

      return compactAccel;
    }

  } // namespace vk
} // namespace rayjoin
