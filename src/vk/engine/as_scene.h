#ifndef RAYJOIN_AS_SCENE_H
#define RAYJOIN_AS_SCENE_H

namespace rayjoin {
namespace vk {
class AccelStructScene {
 public:
  AccelStructScene() : vk_ctx_(GetVkComputeContext()), device_(vk_ctx_.device) { loadFunctionPointers(); }

  ~AccelStructScene() {
    if (tlas_ != VK_NULL_HANDLE) {
      fpDestroyAccelerationStructureKHR(device_, tlas_, nullptr);
      tlas_ = VK_NULL_HANDLE;
    }

    if (blas_ != VK_NULL_HANDLE) {
      fpDestroyAccelerationStructureKHR(device_, blas_, nullptr);
      blas_ = VK_NULL_HANDLE;
    }
  }

  void BuildAccelCustom(const VkDeviceBuf& aabb_buf, uint32_t primitive_count) {
    if (primitive_count == 0) {
      LOG(WARNING) << "AccelStructScene::BuildAccelCustom called with primitive_count=0";
      return;
    }

    auto& ctx = vk_ctx_;

    VkAccelerationStructureGeometryKHR blasGeometry{};
    blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    blasGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    // blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    blasGeometry.flags = 0;

    blasGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    blasGeometry.geometry.aabbs.data.deviceAddress = aabb_buf.DeviceAddress();
    blasGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

    VkAccelerationStructureBuildRangeInfoKHR blasRange{};
    blasRange.primitiveCount = primitive_count;
    const VkAccelerationStructureBuildRangeInfoKHR* blasRangePtr = &blasRange;

    VkAccelerationStructureBuildGeometryInfoKHR blasBuildInfo{};
    blasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    blasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    blasBuildInfo.geometryCount = 1;
    blasBuildInfo.pGeometries = &blasGeometry;

    uint32_t blasPrimCounts[] = {primitive_count};

    VkAccelerationStructureBuildSizesInfoKHR blasSizeInfo{};
    blasSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    fpGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blasBuildInfo, blasPrimCounts, &blasSizeInfo);

    VkDeviceBuf blasBuffer;
    blasBuffer.InitAS(blasSizeInfo.accelerationStructureSize);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo{};
    blasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasCreateInfo.buffer = blasBuffer.Buf();
    blasCreateInfo.size = blasSizeInfo.accelerationStructureSize;
    blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    VK_CHECK(fpCreateAccelerationStructureKHR(device_, &blasCreateInfo, nullptr, &blas));

    VkDeviceBuf blasScratch;
    blasScratch.Init(blasSizeInfo.buildScratchSize);

    blasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    blasBuildInfo.dstAccelerationStructure = blas;
    blasBuildInfo.scratchData.deviceAddress = blasScratch.DeviceAddress();

    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo qp{};
    qp.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qp.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
    qp.queryCount = 1;
    VK_CHECK(vkCreateQueryPool(device_, &qp, nullptr, &queryPool));
    vkResetQueryPool(device_, queryPool, 0, 1);

    VkCommandBuffer cmd = beginOneTime(device_, ctx.cmdPool);

    fpCmdBuildAccelerationStructuresKHR(cmd, 1, &blasBuildInfo, &blasRangePtr);

    VkMemoryBarrier blasBarrier{};
    blasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    blasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    blasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0,
                         1,
                         &blasBarrier,
                         0,
                         nullptr,
                         0,
                         nullptr);

    fpCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &blas, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);

    endSubmitWait(device_, ctx.queue, ctx.cmdPool, cmd);

    VkDeviceSize compactSize = 0;
    VK_CHECK(vkGetQueryPoolResults(
        device_, queryPool, 0, 1, sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT));

    vkDestroyQueryPool(device_, queryPool, nullptr);

    if (compactSize == 0 || compactSize > blasSizeInfo.accelerationStructureSize) {
      compactSize = blasSizeInfo.accelerationStructureSize;
    }

    VkDeviceBuf compactBlasBuffer;
    compactBlasBuffer.InitAS(compactSize);

    VkAccelerationStructureCreateInfoKHR compactBlasCreateInfo{};
    compactBlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    compactBlasCreateInfo.buffer = compactBlasBuffer.Buf();
    compactBlasCreateInfo.size = compactSize;
    compactBlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR compactBlas = VK_NULL_HANDLE;
    VK_CHECK(fpCreateAccelerationStructureKHR(device_, &compactBlasCreateInfo, nullptr, &compactBlas));

    VkCopyAccelerationStructureInfoKHR copyInfo{};
    copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
    copyInfo.src = blas;
    copyInfo.dst = compactBlas;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;

    cmd = beginOneTime(device_, ctx.cmdPool);
    fpCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
    endSubmitWait(device_, ctx.queue, ctx.cmdPool, cmd);

    fpDestroyAccelerationStructureKHR(device_, blas, nullptr);
    blas = VK_NULL_HANDLE;

    VkAccelerationStructureDeviceAddressInfoKHR blasAddrInfo{};
    blasAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    blasAddrInfo.accelerationStructure = compactBlas;

    VkDeviceAddress blasDeviceAddress = fpGetAccelerationStructureDeviceAddressKHR(device_, &blasAddrInfo);

    VkTransformMatrixKHR identityTransform = {{
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
    }};

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform = identityTransform;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    // instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
    instance.flags = 0;
    instance.accelerationStructureReference = blasDeviceAddress;

    VkDeviceBuf instanceBuffer;
    instanceBuffer.Init(sizeof(VkAccelerationStructureInstanceKHR));

    {
      VkStagingBuf staging(sizeof(VkAccelerationStructureInstanceKHR));
      std::vector<VkAccelerationStructureInstanceKHR> tmp = {instance};
      staging.Host2Stage(tmp);
      staging.Stage2Device(instanceBuffer, sizeof(VkAccelerationStructureInstanceKHR));
    }

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.arrayOfPointers = VK_FALSE;
    instancesData.data.deviceAddress = instanceBuffer.DeviceAddress();

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    // tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    tlasGeometry.flags = 0;
    tlasGeometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
    tlasRange.primitiveCount = 1;
    const VkAccelerationStructureBuildRangeInfoKHR* tlasRangePtr = &tlasRange;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
    tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuildInfo.geometryCount = 1;
    tlasBuildInfo.pGeometries = &tlasGeometry;

    uint32_t tlasPrimCounts[] = {1};

    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{};
    tlasSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    fpGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildInfo, tlasPrimCounts, &tlasSizeInfo);

    VkDeviceBuf tlasBuffer;
    tlasBuffer.InitAS(tlasSizeInfo.accelerationStructureSize);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
    tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreateInfo.buffer = tlasBuffer.Buf();
    tlasCreateInfo.size = tlasSizeInfo.accelerationStructureSize;
    tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    VK_CHECK(fpCreateAccelerationStructureKHR(device_, &tlasCreateInfo, nullptr, &tlas));

    VkDeviceBuf tlasScratch;
    tlasScratch.Init(tlasSizeInfo.buildScratchSize);

    tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuildInfo.dstAccelerationStructure = tlas;
    tlasBuildInfo.scratchData.deviceAddress = tlasScratch.DeviceAddress();

    cmd = beginOneTime(device_, ctx.cmdPool);

    fpCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildInfo, &tlasRangePtr);

    VkMemoryBarrier tlasBarrier{};
    tlasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    tlasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    tlasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0,
                         1,
                         &tlasBarrier,
                         0,
                         nullptr,
                         0,
                         nullptr);

    endSubmitWait(device_, ctx.queue, ctx.cmdPool, cmd);

    blas_ = compactBlas;
    tlas_ = tlas;
    blas_buffer_ = std::move(compactBlasBuffer);
    tlas_buffer_ = std::move(tlasBuffer);
    instance_buffer_ = std::move(instanceBuffer);
  }

  [[nodiscard]] VkAccelerationStructureKHR GetTraverseHandle() const { return tlas_; }

 private:
  const VkComputeContext& vk_ctx_;
  VkDevice device_ = VK_NULL_HANDLE;

  PFN_vkCreateAccelerationStructureKHR fpCreateAccelerationStructureKHR = nullptr;
  PFN_vkDestroyAccelerationStructureKHR fpDestroyAccelerationStructureKHR = nullptr;
  PFN_vkGetAccelerationStructureBuildSizesKHR fpGetAccelerationStructureBuildSizesKHR = nullptr;
  PFN_vkCmdBuildAccelerationStructuresKHR fpCmdBuildAccelerationStructuresKHR = nullptr;
  PFN_vkCmdWriteAccelerationStructuresPropertiesKHR fpCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
  PFN_vkCmdCopyAccelerationStructureKHR fpCmdCopyAccelerationStructureKHR = nullptr;
  PFN_vkGetAccelerationStructureDeviceAddressKHR fpGetAccelerationStructureDeviceAddressKHR = nullptr;

  VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;
  VkDeviceBuf tlas_buffer_;

  VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE;
  VkDeviceBuf blas_buffer_;

  VkDeviceBuf instance_buffer_;

  void loadFunctionPointers() {
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

    fpGetAccelerationStructureDeviceAddressKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR"));

    if (!fpCreateAccelerationStructureKHR || !fpDestroyAccelerationStructureKHR || !fpGetAccelerationStructureBuildSizesKHR ||
        !fpCmdBuildAccelerationStructuresKHR || !fpCmdWriteAccelerationStructuresPropertiesKHR || !fpCmdCopyAccelerationStructureKHR ||
        !fpGetAccelerationStructureDeviceAddressKHR) {
      throw std::runtime_error(
          "Vulkan AS functions not loaded. "
          "Ensure device was created with required ray tracing extensions.");
    }
  }
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_AS_SCENE_H
