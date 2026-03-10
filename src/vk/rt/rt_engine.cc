#include "vk/rt/rt_engine.h"

#include <stdexcept>

namespace rayjoin {
namespace vk {

namespace {

// ----------------------------------------------------------------------------
// NEW:
// Small helper to read SPIR-V shader bytecode.
// ----------------------------------------------------------------------------
static std::vector<char> ReadBinaryFile(const char *path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    throw std::runtime_error(std::string("Failed to open shader: ") + path);
  }
  return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// ----------------------------------------------------------------------------
// NEW:
// Simple alignment helper for SBT layout.
// ----------------------------------------------------------------------------
static uint32_t AlignUp(uint32_t x, uint32_t a) { return (x + a - 1u) & ~(a - 1u); }

}  // namespace

RTEngine::RTEngine() {}

RTEngine::~RTEngine() {
  // --------------------------------------------------------------------------
  // NEW:
  // Destroy LSI ray tracing pipeline state first.
  // --------------------------------------------------------------------------
  if (ctx_) {
    if (lsi_pipeline_) {
      vkDestroyPipeline(device_, lsi_pipeline_, nullptr);
      lsi_pipeline_ = VK_NULL_HANDLE;
    }

    if (lsi_desc_pool_) {
      vkDestroyDescriptorPool(device_, lsi_desc_pool_, nullptr);
      lsi_desc_pool_ = VK_NULL_HANDLE;
    }

    if (lsi_pipeline_layout_) {
      vkDestroyPipelineLayout(device_, lsi_pipeline_layout_, nullptr);
      lsi_pipeline_layout_ = VK_NULL_HANDLE;
    }

    if (lsi_desc_set_layout_) {
      vkDestroyDescriptorSetLayout(device_, lsi_desc_set_layout_, nullptr);
      lsi_desc_set_layout_ = VK_NULL_HANDLE;
    }
  }

  // --------------------------------------------------------------------------
  // EXISTING:
  // Destroy all compacted BLAS handles.
  // --------------------------------------------------------------------------
  if (!ctx_ || !fpDestroyAccelerationStructureKHR) return;

  for (auto &e: accels_) {
    // if (e.accel != VK_NULL_HANDLE) {
    //   fpDestroyAccelerationStructureKHR(device_, e.accel, nullptr);
    // }

    if (e.accel != VK_NULL_HANDLE) {
      fpDestroyAccelerationStructureKHR(device_, e.accel, nullptr);
    }
    if (e.blas != VK_NULL_HANDLE) {
      fpDestroyAccelerationStructureKHR(device_, e.blas, nullptr);
    }
  }
}

void RTEngine::Init() {
  ctx_ = &GetVkComputeContext();
  device_ = ctx_->device;
  loadFunctionPointers();
}

void RTEngine::loadFunctionPointers() {
  // --------------------------------------------------------------------------
  // EXISTING:
  // Acceleration structure build / compaction functions.
  // --------------------------------------------------------------------------
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

  // --------------------------------------------------------------------------
  // NEW:
  // Additional RT pipeline / SBT / AS-address functions needed to execute
  // Vulkan ray tracing shaders for LSI queries.
  // --------------------------------------------------------------------------
  fpGetAccelerationStructureDeviceAddressKHR =
      reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR"));

  fpCreateRayTracingPipelinesKHR =
      reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR"));

  fpGetRayTracingShaderGroupHandlesKHR =
      reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR"));

  fpCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));

  if (!fpCreateAccelerationStructureKHR || !fpDestroyAccelerationStructureKHR || !fpGetAccelerationStructureBuildSizesKHR ||
      !fpCmdBuildAccelerationStructuresKHR || !fpCmdWriteAccelerationStructuresPropertiesKHR || !fpCmdCopyAccelerationStructureKHR ||
      !fpGetAccelerationStructureDeviceAddressKHR || !fpCreateRayTracingPipelinesKHR || !fpGetRayTracingShaderGroupHandlesKHR || !fpCmdTraceRaysKHR) {
    throw std::runtime_error(
        "Vulkan RT functions not loaded. "
        "Ensure device was created with required ray tracing extensions.");
  }
}

// ============================================================================
// EXISTING BLAS BUILD CODE
// ============================================================================
// VkAccelerationStructureKHR RTEngine::BuildAccelCustom(const VkDeviceBuf &aabb_buf, uint32_t primitive_count) {
//   auto &ctx = *ctx_;
//
//   VkAccelerationStructureGeometryKHR geometry{};
//   geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
//   geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
//   geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
//
//   geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
//   geometry.geometry.aabbs.data.deviceAddress = aabb_buf.DeviceAddress();
//   geometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);
//
//   VkAccelerationStructureBuildRangeInfoKHR range{};
//   range.primitiveCount = primitive_count;
//   const VkAccelerationStructureBuildRangeInfoKHR *rangePtr = &range;
//
//   VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
//   buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
//   buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
//   buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
//   buildInfo.geometryCount = 1;
//   buildInfo.pGeometries = &geometry;
//
//   uint32_t primCounts[] = {primitive_count};
//
//   VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
//   sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
//
//   fpGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, primCounts, &sizeInfo);
//
//   VkDeviceBuf asBuffer;
//   asBuffer.InitAS(sizeInfo.accelerationStructureSize);
//
//   VkAccelerationStructureCreateInfoKHR createInfo{};
//   createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
//   createInfo.buffer = asBuffer.Buf();
//   createInfo.size = sizeInfo.accelerationStructureSize;
//   createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
//
//   VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
//   fpCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &accel);
//
//   VkDeviceBuf scratch;
//   scratch.Init(sizeInfo.buildScratchSize);
//
//   buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
//   buildInfo.dstAccelerationStructure = accel;
//   buildInfo.scratchData.deviceAddress = scratch.DeviceAddress();
//
//   VkQueryPool queryPool;
//   VkQueryPoolCreateInfo qp{};
//   qp.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
//   qp.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
//   qp.queryCount = 1;
//
//   vkCreateQueryPool(device_, &qp, nullptr, &queryPool);
//   vkResetQueryPool(device_, queryPool, 0, 1);
//
//   VkCommandBuffer cmd = beginOneTime(ctx.device, ctx.cmdPool);
//
//   fpCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &rangePtr);
//
//   VkMemoryBarrier barrier{};
//   barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
//   barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
//   barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
//
//   vkCmdPipelineBarrier(cmd,
//                        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
//                        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
//                        0,
//                        1,
//                        &barrier,
//                        0,
//                        nullptr,
//                        0,
//                        nullptr);
//
//   fpCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &accel, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
//
//   endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);
//
//   VkDeviceSize compactSize = 0;
//   vkGetQueryPoolResults(
//       device_, queryPool, 0, 1, sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
//
//   vkDestroyQueryPool(device_, queryPool, nullptr);
//
//   LOG(INFO) << "Original AS size: " << sizeInfo.accelerationStructureSize;
//   LOG(INFO) << "Compacted size: " << compactSize;
//
//   if (compactSize == 0 || compactSize > sizeInfo.accelerationStructureSize) {
//     compactSize = sizeInfo.accelerationStructureSize;
//   }
//
//   VkDeviceBuf compactBuffer;
//   compactBuffer.InitAS(compactSize);
//
//   VkAccelerationStructureCreateInfoKHR compactInfo{};
//   compactInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
//   compactInfo.buffer = compactBuffer.Buf();
//   compactInfo.size = compactSize;
//   compactInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
//
//   VkAccelerationStructureKHR compactAccel = VK_NULL_HANDLE;
//   fpCreateAccelerationStructureKHR(device_, &compactInfo, nullptr, &compactAccel);
//
//   VkCopyAccelerationStructureInfoKHR copyInfo{};
//   copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
//   copyInfo.src = accel;
//   copyInfo.dst = compactAccel;
//   copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
//
//   cmd = beginOneTime(ctx.device, ctx.cmdPool);
//   fpCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
//   endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);
//
//   fpDestroyAccelerationStructureKHR(device_, accel, nullptr);
//
//   accels_.push_back({compactAccel, std::move(compactBuffer)});
//   return compactAccel;
// }


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Build Accel with TLAS
VkAccelerationStructureKHR RTEngine::BuildAccelCustom(const VkDeviceBuf &aabb_buf, uint32_t primitive_count) {
  auto &ctx = *ctx_;

  // ==========================================================================
  // 1) Build BLAS from AABBs
  // ==========================================================================
  VkAccelerationStructureGeometryKHR blasGeometry{};
  blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  blasGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
  blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

  blasGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
  blasGeometry.geometry.aabbs.data.deviceAddress = aabb_buf.DeviceAddress();
  blasGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

  VkAccelerationStructureBuildRangeInfoKHR blasRange{};
  blasRange.primitiveCount = primitive_count;
  const VkAccelerationStructureBuildRangeInfoKHR *blasRangePtr = &blasRange;

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

  VkCommandBuffer cmd = beginOneTime(ctx.device, ctx.cmdPool);

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

  endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);

  VkDeviceSize compactSize = 0;
  VK_CHECK(vkGetQueryPoolResults(
      device_, queryPool, 0, 1, sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT));

  vkDestroyQueryPool(device_, queryPool, nullptr);

  LOG(INFO) << "Original AS size: " << blasSizeInfo.accelerationStructureSize;
  LOG(INFO) << "Compacted size: " << compactSize;

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

  cmd = beginOneTime(ctx.device, ctx.cmdPool);
  fpCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
  endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);

  fpDestroyAccelerationStructureKHR(device_, blas, nullptr);
  blas = VK_NULL_HANDLE;

  // ==========================================================================
  // 2) Create one instance referencing the BLAS
  // ==========================================================================
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
  instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
  instance.accelerationStructureReference = blasDeviceAddress;

  VkDeviceBuf instanceBuffer;
  instanceBuffer.Init(sizeof(VkAccelerationStructureInstanceKHR));

  {
    VkStagingBuf staging(sizeof(VkAccelerationStructureInstanceKHR));
    std::vector<VkAccelerationStructureInstanceKHR> tmp = {instance};
    staging.Host2Stage(tmp);
    staging.Stage2Device(instanceBuffer, sizeof(VkAccelerationStructureInstanceKHR));
  }

  // ==========================================================================
  // 3) Build TLAS over that one instance
  // ==========================================================================
  VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
  instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  instancesData.arrayOfPointers = VK_FALSE;
  instancesData.data.deviceAddress = instanceBuffer.DeviceAddress();

  VkAccelerationStructureGeometryKHR tlasGeometry{};
  tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  tlasGeometry.geometry.instances = instancesData;

  VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
  tlasRange.primitiveCount = 1;
  const VkAccelerationStructureBuildRangeInfoKHR *tlasRangePtr = &tlasRange;

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

  cmd = beginOneTime(ctx.device, ctx.cmdPool);

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

  endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);

  // ==========================================================================
  // 4) Keep buffers alive and return TLAS
  // ==========================================================================
  // You need accels_ to retain *all* resources backing the returned TLAS scene:
  // - compact BLAS handle + buffer
  // - TLAS handle + buffer
  // - instance buffer
  //
  // Example struct:
  // struct AccelEntry {
  //   VkAccelerationStructureKHR accel;
  //   VkDeviceBuf buffer;
  //   VkAccelerationStructureKHR blas;
  //   VkDeviceBuf blasBuffer;
  //   VkDeviceBuf instanceBuffer;
  // };
  //
  // Then store:
  accels_.push_back({tlas, std::move(tlasBuffer), compactBlas, std::move(compactBlasBuffer), std::move(instanceBuffer)});

  return tlas;
}

// ============================================================================
// NEW LSI PIPELINE SETUP CODE
// ============================================================================

VkShaderModule RTEngine::loadShaderModule(const char *spv_path) {
  auto code = ReadBinaryFile(spv_path);

  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = code.size();
  ci.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule mod = VK_NULL_HANDLE;
  VK_CHECK(vkCreateShaderModule(device_, &ci, nullptr, &mod));
  return mod;
}

// void RTEngine::createLSIDescriptorSetLayout() {
//   VkDescriptorSetLayoutBinding bindings[11]{};
//
//   // 0: acceleration structure
//   bindings[0].binding = 0;
//   bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//   bindings[0].descriptorCount = 1;
//   bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 1: params buffer (StructuredBuffer<LaunchParamsLSI>)
//   bindings[1].binding = 1;
//   bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[1].descriptorCount = 1;
//   bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 2: base edges
//   bindings[2].binding = 2;
//   bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[2].descriptorCount = 1;
//   bindings[2].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 3: base points
//   bindings[3].binding = 3;
//   bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[3].descriptorCount = 1;
//   bindings[3].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 4: eid ranges
//   bindings[4].binding = 4;
//   bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[4].descriptorCount = 1;
//   bindings[4].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 5: query edges
//   bindings[5].binding = 5;
//   bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[5].descriptorCount = 1;
//   bindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 6: query points
//   bindings[6].binding = 6;
//   bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[6].descriptorCount = 1;
//   bindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 7: xsect output
//   bindings[7].binding = 7;
//   bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[7].descriptorCount = 1;
//   bindings[7].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 8: xsect counter
//   bindings[8].binding = 8;
//   bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[8].descriptorCount = 1;
//   bindings[8].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 9: test/profile counter
//   // bindings[9].binding = 9;
//   // bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   // bindings[9].descriptorCount = 1;
//   // bindings[9].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//   bindings[9].binding = 9;
//   bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[9].descriptorCount = 1;
//   bindings[9].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
//
//   // 10: scaling buffer
//   bindings[10].binding = 10;
//   bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[10].descriptorCount = 1;
//   bindings[10].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
//
//   VkDescriptorSetLayoutCreateInfo ci{};
//   ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
//   ci.bindingCount = 11;
//   ci.pBindings = bindings;
//
//   VK_CHECK(vkCreateDescriptorSetLayout(device_, &ci, nullptr, &lsi_desc_set_layout_));
// }

void RTEngine::createLSIDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding bindings[11]{};

  const VkShaderStageFlags hitStages = VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

  // 0: acceleration structure
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages;

  // 1: params buffer
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages;

  // 2: base edges
  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = hitStages;

  // 3: base points
  bindings[3].binding = 3;
  bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[3].descriptorCount = 1;
  bindings[3].stageFlags = hitStages;

  // 4: eid ranges
  bindings[4].binding = 4;
  bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[4].descriptorCount = 1;
  bindings[4].stageFlags = hitStages;

  // 5: query edges
  bindings[5].binding = 5;
  bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[5].descriptorCount = 1;
  bindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages;

  // 6: query points
  bindings[6].binding = 6;
  bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[6].descriptorCount = 1;
  bindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages;

  // 7: xsect output
  bindings[7].binding = 7;
  bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[7].descriptorCount = 1;
  bindings[7].stageFlags = hitStages;

  // 8: xsect counter
  bindings[8].binding = 8;
  bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[8].descriptorCount = 1;
  bindings[8].stageFlags = hitStages;

  // 9: test/profile counter
  bindings[9].binding = 9;
  bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[9].descriptorCount = 1;
  bindings[9].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | hitStages;

  // 10: scaling buffer
  bindings[10].binding = 10;
  bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[10].descriptorCount = 1;
  bindings[10].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages;

  VkDescriptorSetLayoutCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  ci.bindingCount = 11;
  ci.pBindings = bindings;

  VK_CHECK(vkCreateDescriptorSetLayout(device_, &ci, nullptr, &lsi_desc_set_layout_));
}

void RTEngine::createLSIDescriptorPool() {
  VkDescriptorPoolSize poolSizes[2]{};

  poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  poolSizes[0].descriptorCount = 1;

  poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[1].descriptorCount = 10;

  VkDescriptorPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  ci.maxSets = 1;
  ci.poolSizeCount = 2;
  ci.pPoolSizes = poolSizes;

  VK_CHECK(vkCreateDescriptorPool(device_, &ci, nullptr, &lsi_desc_pool_));
}

void RTEngine::allocateLSIDescriptorSet() {
  VkDescriptorSetAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ai.descriptorPool = lsi_desc_pool_;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts = &lsi_desc_set_layout_;

  VK_CHECK(vkAllocateDescriptorSets(device_, &ai, &lsi_desc_set_));
}

void RTEngine::createLSIPipelineLayout() {
  VkPipelineLayoutCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  ci.setLayoutCount = 1;
  ci.pSetLayouts = &lsi_desc_set_layout_;

  VK_CHECK(vkCreatePipelineLayout(device_, &ci, nullptr, &lsi_pipeline_layout_));
}

// void RTEngine::createLSIRTPipeline(const char *rgen_spv, const char *rint_spv, const char *rmiss_spv) {
//   // --------------------------------------------------------------------------
//   // NEW:
//   // Creates a procedural RT pipeline:
//   //   group 0 = raygen
//   //   group 1 = procedural hit group with intersection shader
//   //   group 2 = miss
//   // --------------------------------------------------------------------------
//   VkShaderModule rgen = loadShaderModule(rgen_spv);
//   VkShaderModule rint = loadShaderModule(rint_spv);
//   VkShaderModule rmiss = loadShaderModule(rmiss_spv);
//
//   VkPipelineShaderStageCreateInfo stages[3]{};
//
//   stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//   stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
//   stages[0].module = rgen;
//   stages[0].pName = "main";
//   // stages[0].pName = "raygenMain";
//
//   stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//   stages[1].stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//   stages[1].module = rint;
//   stages[1].pName = "main";
//   // stages[1].pName = "intersectionMain";
//
//   stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//   stages[2].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
//   stages[2].module = rmiss;
//   stages[2].pName = "main";
//   // stages[2].pName = "missMain";
//
//   VkRayTracingShaderGroupCreateInfoKHR groups[3]{};
//
//   // raygen group
//   groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//   groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
//   groups[0].generalShader = 0;
//   groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
//   groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
//   groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;
//
//   // procedural hit group with intersection shader
//   groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//   groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
//   groups[1].generalShader = VK_SHADER_UNUSED_KHR;
//   groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
//   groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
//   groups[1].intersectionShader = 1;
//
//   // miss group
//   groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//   groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
//   groups[2].generalShader = 2;
//   groups[2].closestHitShader = VK_SHADER_UNUSED_KHR;
//   groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
//   groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;
//
//   VkRayTracingPipelineCreateInfoKHR ci{};
//   ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
//   ci.stageCount = 3;
//   ci.pStages = stages;
//   ci.groupCount = 3;
//   ci.pGroups = groups;
//   ci.maxPipelineRayRecursionDepth = 1;
//   ci.layout = lsi_pipeline_layout_;
//
//   VK_CHECK(fpCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &lsi_pipeline_));
//
//   vkDestroyShaderModule(device_, rgen, nullptr);
//   vkDestroyShaderModule(device_, rint, nullptr);
//   vkDestroyShaderModule(device_, rmiss, nullptr);
//
//   buildLSISBT(3);
// }
void RTEngine::createLSIRTPipeline(const char *rgen_spv, const char *rint_spv, const char *rahit_spv, const char *rchit_spv, const char *rmiss_spv) {
  // Stages:
  //   0 = raygen
  //   1 = intersection
  //   2 = any-hit
  //   3 = closest-hit
  //   4 = miss
  //
  // Groups:
  //   0 = raygen general group
  //   1 = procedural hit group {intersection, any-hit, closest-hit}
  //   2 = miss general group

  VkShaderModule rgen = loadShaderModule(rgen_spv);
  VkShaderModule rint = loadShaderModule(rint_spv);
  VkShaderModule rahit = loadShaderModule(rahit_spv);
  VkShaderModule rchit = loadShaderModule(rchit_spv);
  VkShaderModule rmiss = loadShaderModule(rmiss_spv);

  VkPipelineShaderStageCreateInfo stages[5]{};

  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  stages[0].module = rgen;
  stages[0].pName = "main";

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
  stages[1].module = rint;
  stages[1].pName = "main";

  stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[2].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
  stages[2].module = rahit;
  stages[2].pName = "main";

  stages[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[3].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  stages[3].module = rchit;
  stages[3].pName = "main";

  stages[4].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[4].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[4].module = rmiss;
  stages[4].pName = "main";

  VkRayTracingShaderGroupCreateInfoKHR groups[3]{};

  // Group 0: raygen
  groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  groups[0].generalShader = 0;
  groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
  groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
  groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

  // Group 1: procedural hit group
  groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
  groups[1].generalShader = VK_SHADER_UNUSED_KHR;
  groups[1].closestHitShader = 3;
  groups[1].anyHitShader = 2;
  groups[1].intersectionShader = 1;

  // Group 2: miss
  groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  groups[2].generalShader = 4;
  groups[2].closestHitShader = VK_SHADER_UNUSED_KHR;
  groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
  groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

  VkRayTracingPipelineCreateInfoKHR ci{};
  ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
  ci.stageCount = 5;
  ci.pStages = stages;
  ci.groupCount = 3;
  ci.pGroups = groups;
  ci.maxPipelineRayRecursionDepth = 1;
  ci.layout = lsi_pipeline_layout_;

  VK_CHECK(fpCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &lsi_pipeline_));

  vkDestroyShaderModule(device_, rgen, nullptr);
  vkDestroyShaderModule(device_, rint, nullptr);
  vkDestroyShaderModule(device_, rahit, nullptr);
  vkDestroyShaderModule(device_, rchit, nullptr);
  vkDestroyShaderModule(device_, rmiss, nullptr);

  // Still 3 groups in the SBT: raygen / hit / miss
  buildLSISBT(3);
}


// void RTEngine::buildLSISBT(uint32_t group_count) {
//   // --------------------------------------------------------------------------
//   // NEW:
//   // Reads shader group handles from the RT pipeline and builds one SBT buffer.
//   // --------------------------------------------------------------------------
//   VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
//   rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
//
//   VkPhysicalDeviceProperties2 props2{};
//   props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
//   props2.pNext = &rtProps;
//
//   vkGetPhysicalDeviceProperties2(ctx_->phys, &props2);
//
//   uint32_t handleSize = rtProps.shaderGroupHandleSize;
//   uint32_t handleAlign = rtProps.shaderGroupHandleAlignment;
//   uint32_t baseAlign = rtProps.shaderGroupBaseAlignment;
//
//   lsi_sbt_stride_ = AlignUp(handleSize, handleAlign);
//   uint32_t sbtSize = AlignUp(group_count * lsi_sbt_stride_, baseAlign);
//
//   std::vector<unsigned char> handles(group_count * handleSize);
//   VK_CHECK(fpGetRayTracingShaderGroupHandlesKHR(device_, lsi_pipeline_, 0, group_count, static_cast<uint32_t>(handles.size()), handles.data()));
//
//   std::vector<unsigned char> sbt(sbtSize, 0);
//   for (uint32_t i = 0; i < group_count; ++i) {
//     std::memcpy(sbt.data() + i * lsi_sbt_stride_, handles.data() + i * handleSize, handleSize);
//   }
//
//   lsi_sbt_buf_.Init(sbtSize);
//
//   VkStagingBuf staging(sbtSize);
//   staging.Host2Stage(sbt);
//   staging.Stage2Device(lsi_sbt_buf_, sbtSize);
// }

void RTEngine::buildLSISBT(uint32_t group_count) {
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
  rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

  VkPhysicalDeviceProperties2 props2{};
  props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props2.pNext = &rtProps;

  vkGetPhysicalDeviceProperties2(ctx_->phys, &props2);

  const uint32_t handleSize = rtProps.shaderGroupHandleSize;
  const uint32_t handleAlign = rtProps.shaderGroupHandleAlignment;
  const uint32_t baseAlign = rtProps.shaderGroupBaseAlignment;

  const uint32_t handleSizeAligned = AlignUp(handleSize, handleAlign);
  const uint32_t recordSize = AlignUp(handleSizeAligned, baseAlign);

  lsi_sbt_stride_ = recordSize;

  const uint32_t sbtSize = group_count * recordSize;

  std::vector<unsigned char> handles(group_count * handleSize);
  VK_CHECK(fpGetRayTracingShaderGroupHandlesKHR(device_, lsi_pipeline_, 0, group_count, static_cast<uint32_t>(handles.size()), handles.data()));

  std::vector<unsigned char> sbt(sbtSize, 0);
  for (uint32_t i = 0; i < group_count; ++i) {
    std::memcpy(sbt.data() + i * recordSize, handles.data() + i * handleSize, handleSize);
  }

  lsi_sbt_buf_.InitSBT(sbtSize);

  VkStagingBuf staging(sbtSize);
  staging.Host2Stage(sbt);
  staging.Stage2Device(lsi_sbt_buf_, sbtSize);

  LOG(INFO) << "SBT props:"
            << " handleSize=" << handleSize << " handleAlign=" << handleAlign << " baseAlign=" << baseAlign
            << " handleSizeAligned=" << handleSizeAligned << " recordSize=" << recordSize << " sbtSize=" << sbtSize;
}

// void RTEngine::InitLSIPipeline(const char *rgen_spv, const char *rint_spv, const char *rmiss_spv) {
//   // --------------------------------------------------------------------------
//   // NEW:
//   // One-time setup for the LSI RT pipeline.
//   // --------------------------------------------------------------------------
//   createLSIDescriptorSetLayout();
//   createLSIDescriptorPool();
//   allocateLSIDescriptorSet();
//   createLSIPipelineLayout();
//   createLSIRTPipeline(rgen_spv, rint_spv, rmiss_spv);
//
//   if (lsi_desc_set_layout_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_desc_set_layout_ is null");
//   }
//   if (lsi_desc_pool_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_desc_pool_ is null");
//   }
//   if (lsi_desc_set_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_desc_set_ is null");
//   }
//   if (lsi_pipeline_layout_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_pipeline_layout_ is null");
//   }
//   if (lsi_pipeline_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_pipeline_ is null");
//   }
// }

void RTEngine::InitLSIPipeline(const char *rgen_spv, const char *rint_spv, const char *rahit_spv, const char *rchit_spv, const char *rmiss_spv) {
  createLSIDescriptorSetLayout();
  createLSIDescriptorPool();
  allocateLSIDescriptorSet();
  createLSIPipelineLayout();
  createLSIRTPipeline(rgen_spv, rint_spv, rahit_spv, rchit_spv, rmiss_spv);

  if (lsi_desc_set_layout_ == VK_NULL_HANDLE) {
    throw std::runtime_error("InitLSIPipeline(): lsi_desc_set_layout_ is null");
  }
  if (lsi_desc_pool_ == VK_NULL_HANDLE) {
    throw std::runtime_error("InitLSIPipeline(): lsi_desc_pool_ is null");
  }
  if (lsi_desc_set_ == VK_NULL_HANDLE) {
    throw std::runtime_error("InitLSIPipeline(): lsi_desc_set_ is null");
  }
  if (lsi_pipeline_layout_ == VK_NULL_HANDLE) {
    throw std::runtime_error("InitLSIPipeline(): lsi_pipeline_layout_ is null");
  }
  if (lsi_pipeline_ == VK_NULL_HANDLE) {
    throw std::runtime_error("InitLSIPipeline(): lsi_pipeline_ is null");
  }
}

// ============================================================================
// NEW LSI QUERY BINDING + EXECUTION CODE
// ============================================================================
void RTEngine::SetLSIQuery(VkAccelerationStructureKHR handle,
                           const VkDeviceBuf &eid_range_buf,
                           const VkDeviceBuf &base_points_buf,
                           const VkDeviceBuf &base_edges_buf,
                           const VkDeviceBuf &query_points_buf,
                           const VkDeviceBuf &query_edges_buf,
                           const VkDeviceBuf &scaling_buf,
                           const VkDeviceBuf &xsect_buf,
                           const VkDeviceBuf &xsect_counter_buf,  // NEW
                           const VkDeviceBuf &prof_counter_buf,
                           uint32_t xsect_capacity,
                           int query_map_id,
                           uint32_t query_edge_count) {
  if (device_ == VK_NULL_HANDLE) {
    throw std::runtime_error("SetLSIQuery(): device_ is null");
  }
  if (lsi_desc_set_ == VK_NULL_HANDLE) {
    throw std::runtime_error(
        "SetLSIQuery(): LSI pipeline/descriptors are not initialized. "
        "Call InitLSIPipeline() before Query().");
  }

  lsi_query_.handle = handle;
  lsi_query_.eid_range_buf = &eid_range_buf;
  lsi_query_.base_points_buf = &base_points_buf;
  lsi_query_.base_edges_buf = &base_edges_buf;
  lsi_query_.query_points_buf = &query_points_buf;
  lsi_query_.query_edges_buf = &query_edges_buf;
  lsi_query_.scaling_buf = &scaling_buf;  // NEW
  lsi_query_.xsect_buf = &xsect_buf;
  lsi_query_.xsect_counter_buf = &xsect_counter_buf;
  lsi_query_.prof_counter_buf = &prof_counter_buf;
  lsi_query_.xsect_capacity = xsect_capacity;
  lsi_query_.query_map_id = query_map_id;
  lsi_query_.query_edge_count = query_edge_count;

  struct LaunchParamsLSI {
    int query_map_id;
    uint32_t query_edge_count;
    uint32_t xsect_capacity;
    uint32_t _pad0;
  } params{query_map_id, query_edge_count, xsect_capacity, 0};

  uploadLSIParams(params);
  updateLSIDescriptors();
}

void RTEngine::updateLSIDescriptors() {
  if (device_ == VK_NULL_HANDLE) {
    throw std::runtime_error("RTEngine::updateLSIDescriptors(): device_ is null");
  }
  if (lsi_desc_set_ == VK_NULL_HANDLE) {
    throw std::runtime_error("RTEngine::updateLSIDescriptors(): lsi_desc_set_ is null");
  }
  if (lsi_query_.handle == VK_NULL_HANDLE) {
    throw std::runtime_error("RTEngine::updateLSIDescriptors(): lsi_query_.handle is null");
  }
  if (lsi_params_buf_.Buf() == VK_NULL_HANDLE) {
    throw std::runtime_error("RTEngine::updateLSIDescriptors(): lsi_params_buf_ is null");
  }
  // if (!lsi_query_.eid_range_buf || !lsi_query_.base_points_buf || !lsi_query_.base_edges_buf || !lsi_query_.query_points_buf ||
  //     !lsi_query_.query_edges_buf || !lsi_query_.scaling_buf || !lsi_query_.xsect_buf || !lsi_query_.prof_counter_buf) {
  //   throw std::runtime_error("RTEngine::updateLSIDescriptors(): one or more query buffers are null");
  // }
  if (!lsi_query_.eid_range_buf || !lsi_query_.base_points_buf || !lsi_query_.base_edges_buf || !lsi_query_.query_points_buf ||
      !lsi_query_.query_edges_buf || !lsi_query_.scaling_buf || !lsi_query_.xsect_buf || !lsi_query_.xsect_counter_buf ||
      !lsi_query_.prof_counter_buf) {
    throw std::runtime_error("RTEngine::updateLSIDescriptors(): one or more query buffers are null");
  }

  VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
  asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
  asInfo.accelerationStructureCount = 1;
  asInfo.pAccelerationStructures = &lsi_query_.handle;

  VkDescriptorBufferInfo paramInfo{};
  paramInfo.buffer = lsi_params_buf_.Buf();
  paramInfo.offset = 0;
  paramInfo.range = VK_WHOLE_SIZE;

  VkDescriptorBufferInfo baseEdgesInfo{};
  baseEdgesInfo.buffer = lsi_query_.base_edges_buf->Buf();
  baseEdgesInfo.offset = 0;
  baseEdgesInfo.range = VK_WHOLE_SIZE;

  VkDescriptorBufferInfo basePointsInfo{};
  basePointsInfo.buffer = lsi_query_.base_points_buf->Buf();
  basePointsInfo.offset = 0;
  basePointsInfo.range = VK_WHOLE_SIZE;

  VkDescriptorBufferInfo eidRangeInfo{};
  eidRangeInfo.buffer = lsi_query_.eid_range_buf->Buf();
  eidRangeInfo.offset = 0;
  eidRangeInfo.range = VK_WHOLE_SIZE;

  VkDescriptorBufferInfo queryEdgesInfo{};
  queryEdgesInfo.buffer = lsi_query_.query_edges_buf->Buf();
  queryEdgesInfo.offset = 0;
  queryEdgesInfo.range = VK_WHOLE_SIZE;

  VkDescriptorBufferInfo queryPointsInfo{};
  queryPointsInfo.buffer = lsi_query_.query_points_buf->Buf();
  queryPointsInfo.offset = 0;
  queryPointsInfo.range = VK_WHOLE_SIZE;

  VkDescriptorBufferInfo xsectInfo{};
  xsectInfo.buffer = lsi_query_.xsect_buf->Buf();
  xsectInfo.offset = 0;
  xsectInfo.range = VK_WHOLE_SIZE;

  // binding 8 = xsect append counter
  // For now reuse prof_counter_buf if you do not yet have a dedicated counter.
  // Better: create a dedicated xsect counter buffer later.
  VkDescriptorBufferInfo xsectCounterInfo{};
  // xsectCounterInfo.buffer = lsi_query_.prof_counter_buf->Buf();
  xsectCounterInfo.buffer = lsi_query_.xsect_counter_buf->Buf();
  xsectCounterInfo.offset = 0;
  xsectCounterInfo.range = sizeof(uint32_t);

  // binding 9 = test/profile counter
  VkDescriptorBufferInfo testCounterInfo{};
  testCounterInfo.buffer = lsi_query_.prof_counter_buf->Buf();
  testCounterInfo.offset = 0;
  testCounterInfo.range = VK_WHOLE_SIZE;

  VkDescriptorBufferInfo scalingInfo{};
  scalingInfo.buffer = lsi_query_.scaling_buf->Buf();
  scalingInfo.offset = 0;
  scalingInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writes[11]{};

  // 0: AS
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = &asInfo;
  writes[0].dstSet = lsi_desc_set_;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

  // 1: params
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = lsi_desc_set_;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &paramInfo;

  // 2: base edges
  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = lsi_desc_set_;
  writes[2].dstBinding = 2;
  writes[2].descriptorCount = 1;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[2].pBufferInfo = &baseEdgesInfo;

  // 3: base points
  writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[3].dstSet = lsi_desc_set_;
  writes[3].dstBinding = 3;
  writes[3].descriptorCount = 1;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[3].pBufferInfo = &basePointsInfo;

  // 4: eid ranges
  writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[4].dstSet = lsi_desc_set_;
  writes[4].dstBinding = 4;
  writes[4].descriptorCount = 1;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[4].pBufferInfo = &eidRangeInfo;

  // 5: query edges
  writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[5].dstSet = lsi_desc_set_;
  writes[5].dstBinding = 5;
  writes[5].descriptorCount = 1;
  writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[5].pBufferInfo = &queryEdgesInfo;

  // 6: query points
  writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[6].dstSet = lsi_desc_set_;
  writes[6].dstBinding = 6;
  writes[6].descriptorCount = 1;
  writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[6].pBufferInfo = &queryPointsInfo;

  // 7: xsect output
  writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[7].dstSet = lsi_desc_set_;
  writes[7].dstBinding = 7;
  writes[7].descriptorCount = 1;
  writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[7].pBufferInfo = &xsectInfo;

  // 8: xsect counter
  writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[8].dstSet = lsi_desc_set_;
  writes[8].dstBinding = 8;
  writes[8].descriptorCount = 1;
  writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[8].pBufferInfo = &xsectCounterInfo;

  // 9: test/profile counter
  writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[9].dstSet = lsi_desc_set_;
  writes[9].dstBinding = 9;
  writes[9].descriptorCount = 1;
  writes[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[9].pBufferInfo = &testCounterInfo;

  // 10: scaling
  writes[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[10].dstSet = lsi_desc_set_;
  writes[10].dstBinding = 10;
  writes[10].descriptorCount = 1;
  writes[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[10].pBufferInfo = &scalingInfo;

  vkUpdateDescriptorSets(device_, 11, writes, 0, nullptr);
}

// void RTEngine::RunLSI() {
//   // --------------------------------------------------------------------------
//   // NEW:
//   // Executes the LSI RT pipeline using the SBT and query state prepared
//   // earlier.
//   //
//   // This is the Vulkan equivalent of:
//   //   optixLaunch(..., num_query_edges, 1, 1)
//   // --------------------------------------------------------------------------
//   if (lsi_pipeline_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("RunLSI(): LSI pipeline not initialized");
//   }
//
//   if (lsi_query_.handle == VK_NULL_HANDLE) {
//     throw std::runtime_error("RunLSI(): no AS handle set");
//   }
//
//   VkCommandBuffer cmd = beginOneTime(device_, ctx_->cmdPool);
//
//   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lsi_pipeline_);
//
//   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lsi_pipeline_layout_, 0, 1, &lsi_desc_set_, 0, nullptr);
//
//   VkStridedDeviceAddressRegionKHR rgen{};
//   VkStridedDeviceAddressRegionKHR miss{};
//   VkStridedDeviceAddressRegionKHR hit{};
//   VkStridedDeviceAddressRegionKHR call{};
//
//   VkDeviceAddress sbtAddr = lsi_sbt_buf_.DeviceAddress();
//
//   rgen.deviceAddress = sbtAddr + 0 * lsi_sbt_stride_;
//   rgen.stride = lsi_sbt_stride_;
//   rgen.size = lsi_sbt_stride_;
//
//   hit.deviceAddress = sbtAddr + 1 * lsi_sbt_stride_;
//   hit.stride = lsi_sbt_stride_;
//   hit.size = lsi_sbt_stride_;
//
//   miss.deviceAddress = sbtAddr + 2 * lsi_sbt_stride_;
//   miss.stride = lsi_sbt_stride_;
//   miss.size = lsi_sbt_stride_;
//
//   call.deviceAddress = 0;
//   call.stride = 0;
//   call.size = 0;
//
//   fpCmdTraceRaysKHR(cmd, &rgen, &miss, &hit, &call, lsi_query_.query_edge_count, 1, 1);
//
//   endSubmitWait(device_, ctx_->queue, ctx_->cmdPool, cmd);
// }

void RTEngine::RunLSI() {
  if (lsi_pipeline_ == VK_NULL_HANDLE) {
    throw std::runtime_error("RunLSI(): LSI pipeline not initialized");
  }

  if (lsi_query_.handle == VK_NULL_HANDLE) {
    throw std::runtime_error("RunLSI(): no AS handle set");
  }

  VkCommandBuffer cmd = beginOneTime(device_, ctx_->cmdPool);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lsi_pipeline_);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lsi_pipeline_layout_, 0, 1, &lsi_desc_set_, 0, nullptr);

  VkStridedDeviceAddressRegionKHR rgen{};
  VkStridedDeviceAddressRegionKHR miss{};
  VkStridedDeviceAddressRegionKHR hit{};
  VkStridedDeviceAddressRegionKHR call{};

  VkDeviceAddress sbtAddr = lsi_sbt_buf_.DeviceAddress();

  rgen.deviceAddress = sbtAddr + 0 * lsi_sbt_stride_;
  rgen.stride = lsi_sbt_stride_;
  rgen.size = lsi_sbt_stride_;

  hit.deviceAddress = sbtAddr + 1 * lsi_sbt_stride_;
  hit.stride = lsi_sbt_stride_;
  hit.size = lsi_sbt_stride_;

  miss.deviceAddress = sbtAddr + 2 * lsi_sbt_stride_;
  miss.stride = lsi_sbt_stride_;
  miss.size = lsi_sbt_stride_;

  call.deviceAddress = 0;
  call.stride = 0;
  call.size = 0;

  LOG(INFO) << "SBT addrs:"
            << " base=" << sbtAddr << " rgen=" << rgen.deviceAddress << " hit=" << hit.deviceAddress << " miss=" << miss.deviceAddress
            << " stride=" << lsi_sbt_stride_;

  fpCmdTraceRaysKHR(cmd, &rgen, &miss, &hit, &call, lsi_query_.query_edge_count, 1, 1);

  endSubmitWait(device_, ctx_->queue, ctx_->cmdPool, cmd);
}

}  // namespace vk
}  // namespace rayjoin


// No Miss
///////////////////////////////////////////////////////////////
// #include "vk/rt/rt_engine.h"
//
// #include <stdexcept>
//
// namespace rayjoin {
// namespace vk {
//
// namespace {
//
// static std::vector<char> ReadBinaryFile(const char *path) {
//   std::ifstream f(path, std::ios::binary);
//   if (!f) {
//     throw std::runtime_error(std::string("Failed to open shader: ") + path);
//   }
//
//   return std::vector<char>((std::istreambuf_iterator<char>(f)),
//                            std::istreambuf_iterator<char>());
// }
//
// static uint32_t AlignUp(uint32_t x, uint32_t a) {
//   return (x + a - 1u) & ~(a - 1u);
// }
//
// } // namespace
//
// RTEngine::RTEngine() {}
//
// RTEngine::~RTEngine() {
//   if (ctx_) {
//     if (lsi_pipeline_) {
//       vkDestroyPipeline(device_, lsi_pipeline_, nullptr);
//       lsi_pipeline_ = VK_NULL_HANDLE;
//     }
//
//     if (lsi_desc_pool_) {
//       vkDestroyDescriptorPool(device_, lsi_desc_pool_, nullptr);
//       lsi_desc_pool_ = VK_NULL_HANDLE;
//     }
//
//     if (lsi_pipeline_layout_) {
//       vkDestroyPipelineLayout(device_, lsi_pipeline_layout_, nullptr);
//       lsi_pipeline_layout_ = VK_NULL_HANDLE;
//     }
//
//     if (lsi_desc_set_layout_) {
//       vkDestroyDescriptorSetLayout(device_, lsi_desc_set_layout_, nullptr);
//       lsi_desc_set_layout_ = VK_NULL_HANDLE;
//     }
//   }
//
//   if (!ctx_ || !fpDestroyAccelerationStructureKHR)
//     return;
//
//   for (auto &e : accels_) {
//     if (e.accel != VK_NULL_HANDLE) {
//       fpDestroyAccelerationStructureKHR(device_, e.accel, nullptr);
//     }
//     if (e.blas != VK_NULL_HANDLE) {
//       fpDestroyAccelerationStructureKHR(device_, e.blas, nullptr);
//     }
//   }
// }
//
// void RTEngine::Init() {
//   ctx_ = &GetVkComputeContext();
//   device_ = ctx_->device;
//   loadFunctionPointers();
// }
//
// void RTEngine::loadFunctionPointers() {
//   fpCreateAccelerationStructureKHR =
//       reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
//           vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR"));
//
//   fpDestroyAccelerationStructureKHR =
//       reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
//           vkGetDeviceProcAddr(device_, "vkDestroyAccelerationStructureKHR"));
//
//   fpGetAccelerationStructureBuildSizesKHR =
//       reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
//           vkGetDeviceProcAddr(device_,
//                               "vkGetAccelerationStructureBuildSizesKHR"));
//
//   fpCmdBuildAccelerationStructuresKHR =
//       reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
//           vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR"));
//
//   fpCmdWriteAccelerationStructuresPropertiesKHR =
//       reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
//           vkGetDeviceProcAddr(
//               device_, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
//
//   fpCmdCopyAccelerationStructureKHR =
//       reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
//           vkGetDeviceProcAddr(device_, "vkCmdCopyAccelerationStructureKHR"));
//
//   fpGetAccelerationStructureDeviceAddressKHR =
//       reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
//           vkGetDeviceProcAddr(device_,
//                               "vkGetAccelerationStructureDeviceAddressKHR"));
//
//   fpCreateRayTracingPipelinesKHR =
//       reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
//           vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR"));
//
//   fpGetRayTracingShaderGroupHandlesKHR =
//       reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
//           vkGetDeviceProcAddr(device_,
//                               "vkGetRayTracingShaderGroupHandlesKHR"));
//
//   fpCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
//       vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));
//
//   if (!fpCreateAccelerationStructureKHR ||
//       !fpDestroyAccelerationStructureKHR ||
//       !fpGetAccelerationStructureBuildSizesKHR ||
//       !fpCmdBuildAccelerationStructuresKHR ||
//       !fpCmdWriteAccelerationStructuresPropertiesKHR ||
//       !fpCmdCopyAccelerationStructureKHR ||
//       !fpGetAccelerationStructureDeviceAddressKHR ||
//       !fpCreateRayTracingPipelinesKHR ||
//       !fpGetRayTracingShaderGroupHandlesKHR || !fpCmdTraceRaysKHR) {
//     throw std::runtime_error(
//         "Vulkan RT functions not loaded. "
//         "Ensure device was created with required ray tracing extensions.");
//   }
// }
//
// VkAccelerationStructureKHR RTEngine::BuildAccelCustom(
//     const VkDeviceBuf &aabb_buf, uint32_t primitive_count) {
//   auto &ctx = *ctx_;
//
//   // ==========================================================================
//   // 1) Build BLAS from AABBs
//   // ==========================================================================
//   VkAccelerationStructureGeometryKHR blasGeometry{};
//   blasGeometry.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
//   blasGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
//   blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
//
//   blasGeometry.geometry.aabbs.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
//   blasGeometry.geometry.aabbs.data.deviceAddress = aabb_buf.DeviceAddress();
//   blasGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);
//
//   VkAccelerationStructureBuildRangeInfoKHR blasRange{};
//   blasRange.primitiveCount = primitive_count;
//   const VkAccelerationStructureBuildRangeInfoKHR *blasRangePtr = &blasRange;
//
//   VkAccelerationStructureBuildGeometryInfoKHR blasBuildInfo{};
//   blasBuildInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
//   blasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
//   blasBuildInfo.flags =
//       VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
//       VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
//   blasBuildInfo.geometryCount = 1;
//   blasBuildInfo.pGeometries = &blasGeometry;
//
//   uint32_t blasPrimCounts[] = {primitive_count};
//
//   VkAccelerationStructureBuildSizesInfoKHR blasSizeInfo{};
//   blasSizeInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
//
//   fpGetAccelerationStructureBuildSizesKHR(
//       device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blasBuildInfo,
//       blasPrimCounts, &blasSizeInfo);
//
//   VkDeviceBuf blasBuffer;
//   blasBuffer.InitAS(blasSizeInfo.accelerationStructureSize);
//
//   VkAccelerationStructureCreateInfoKHR blasCreateInfo{};
//   blasCreateInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
//   blasCreateInfo.buffer = blasBuffer.Buf();
//   blasCreateInfo.size = blasSizeInfo.accelerationStructureSize;
//   blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
//
//   VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
//   VK_CHECK(
//       fpCreateAccelerationStructureKHR(device_, &blasCreateInfo, nullptr, &blas));
//
//   VkDeviceBuf blasScratch;
//   blasScratch.Init(blasSizeInfo.buildScratchSize);
//
//   blasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
//   blasBuildInfo.dstAccelerationStructure = blas;
//   blasBuildInfo.scratchData.deviceAddress = blasScratch.DeviceAddress();
//
//   VkQueryPool queryPool = VK_NULL_HANDLE;
//   VkQueryPoolCreateInfo qp{};
//   qp.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
//   qp.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
//   qp.queryCount = 1;
//   VK_CHECK(vkCreateQueryPool(device_, &qp, nullptr, &queryPool));
//   vkResetQueryPool(device_, queryPool, 0, 1);
//
//   VkCommandBuffer cmd = beginOneTime(ctx.device, ctx.cmdPool);
//
//   fpCmdBuildAccelerationStructuresKHR(cmd, 1, &blasBuildInfo, &blasRangePtr);
//
//   VkMemoryBarrier blasBarrier{};
//   blasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
//   blasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
//   blasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
//
//   vkCmdPipelineBarrier(cmd,
//                        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
//                        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0,
//                        1, &blasBarrier, 0, nullptr, 0, nullptr);
//
//   fpCmdWriteAccelerationStructuresPropertiesKHR(
//       cmd, 1, &blas, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
//       queryPool, 0);
//
//   endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);
//
//   VkDeviceSize compactSize = 0;
//   VK_CHECK(vkGetQueryPoolResults(device_, queryPool, 0, 1,
//                                  sizeof(VkDeviceSize), &compactSize,
//                                  sizeof(VkDeviceSize),
//                                  VK_QUERY_RESULT_WAIT_BIT |
//                                      VK_QUERY_RESULT_64_BIT));
//
//   vkDestroyQueryPool(device_, queryPool, nullptr);
//
//   LOG(INFO) << "Original AS size: " << blasSizeInfo.accelerationStructureSize;
//   LOG(INFO) << "Compacted size: " << compactSize;
//
//   if (compactSize == 0 ||
//       compactSize > blasSizeInfo.accelerationStructureSize) {
//     compactSize = blasSizeInfo.accelerationStructureSize;
//   }
//
//   VkDeviceBuf compactBlasBuffer;
//   compactBlasBuffer.InitAS(compactSize);
//
//   VkAccelerationStructureCreateInfoKHR compactBlasCreateInfo{};
//   compactBlasCreateInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
//   compactBlasCreateInfo.buffer = compactBlasBuffer.Buf();
//   compactBlasCreateInfo.size = compactSize;
//   compactBlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
//
//   VkAccelerationStructureKHR compactBlas = VK_NULL_HANDLE;
//   VK_CHECK(fpCreateAccelerationStructureKHR(device_, &compactBlasCreateInfo,
//                                             nullptr, &compactBlas));
//
//   VkCopyAccelerationStructureInfoKHR copyInfo{};
//   copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
//   copyInfo.src = blas;
//   copyInfo.dst = compactBlas;
//   copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
//
//   cmd = beginOneTime(ctx.device, ctx.cmdPool);
//   fpCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
//   endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);
//
//   fpDestroyAccelerationStructureKHR(device_, blas, nullptr);
//   blas = VK_NULL_HANDLE;
//
//   // ==========================================================================
//   // 2) Create one instance referencing the BLAS
//   // ==========================================================================
//   VkAccelerationStructureDeviceAddressInfoKHR blasAddrInfo{};
//   blasAddrInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
//   blasAddrInfo.accelerationStructure = compactBlas;
//
//   VkDeviceAddress blasDeviceAddress =
//       fpGetAccelerationStructureDeviceAddressKHR(device_, &blasAddrInfo);
//
//   VkTransformMatrixKHR identityTransform = {{
//       1.0f, 0.0f, 0.0f, 0.0f, //
//       0.0f, 1.0f, 0.0f, 0.0f, //
//       0.0f, 0.0f, 1.0f, 0.0f,
//   }};
//
//   VkAccelerationStructureInstanceKHR instance{};
//   instance.transform = identityTransform;
//   instance.instanceCustomIndex = 0;
//   instance.mask = 0xFF;
//   instance.instanceShaderBindingTableRecordOffset = 0;
//   instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
//   instance.accelerationStructureReference = blasDeviceAddress;
//
//   VkDeviceBuf instanceBuffer;
//   instanceBuffer.Init(sizeof(VkAccelerationStructureInstanceKHR));
//
//   {
//     VkStagingBuf staging(sizeof(VkAccelerationStructureInstanceKHR));
//     std::vector<VkAccelerationStructureInstanceKHR> tmp = {instance};
//     staging.Host2Stage(tmp);
//     staging.Stage2Device(instanceBuffer,
//                          sizeof(VkAccelerationStructureInstanceKHR));
//   }
//
//   // ==========================================================================
//   // 3) Build TLAS over that one instance
//   // ==========================================================================
//   VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
//   instancesData.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
//   instancesData.arrayOfPointers = VK_FALSE;
//   instancesData.data.deviceAddress = instanceBuffer.DeviceAddress();
//
//   VkAccelerationStructureGeometryKHR tlasGeometry{};
//   tlasGeometry.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
//   tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
//   tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
//   tlasGeometry.geometry.instances = instancesData;
//
//   VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
//   tlasRange.primitiveCount = 1;
//   const VkAccelerationStructureBuildRangeInfoKHR *tlasRangePtr = &tlasRange;
//
//   VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
//   tlasBuildInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
//   tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
//   tlasBuildInfo.flags =
//       VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
//   tlasBuildInfo.geometryCount = 1;
//   tlasBuildInfo.pGeometries = &tlasGeometry;
//
//   uint32_t tlasPrimCounts[] = {1};
//
//   VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{};
//   tlasSizeInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
//
//   fpGetAccelerationStructureBuildSizesKHR(
//       device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildInfo,
//       tlasPrimCounts, &tlasSizeInfo);
//
//   VkDeviceBuf tlasBuffer;
//   tlasBuffer.InitAS(tlasSizeInfo.accelerationStructureSize);
//
//   VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
//   tlasCreateInfo.sType =
//       VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
//   tlasCreateInfo.buffer = tlasBuffer.Buf();
//   tlasCreateInfo.size = tlasSizeInfo.accelerationStructureSize;
//   tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
//
//   VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
//   VK_CHECK(
//       fpCreateAccelerationStructureKHR(device_, &tlasCreateInfo, nullptr, &tlas));
//
//   VkDeviceBuf tlasScratch;
//   tlasScratch.Init(tlasSizeInfo.buildScratchSize);
//
//   tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
//   tlasBuildInfo.dstAccelerationStructure = tlas;
//   tlasBuildInfo.scratchData.deviceAddress = tlasScratch.DeviceAddress();
//
//   cmd = beginOneTime(ctx.device, ctx.cmdPool);
//
//   fpCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildInfo, &tlasRangePtr);
//
//   VkMemoryBarrier tlasBarrier{};
//   tlasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
//   tlasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
//   tlasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
//
//   vkCmdPipelineBarrier(cmd,
//                        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
//                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1,
//                        &tlasBarrier, 0, nullptr, 0, nullptr);
//
//   endSubmitWait(ctx.device, ctx.queue, ctx.cmdPool, cmd);
//
//   accels_.push_back({tlas,
//                      std::move(tlasBuffer),
//                      compactBlas,
//                      std::move(compactBlasBuffer),
//                      std::move(instanceBuffer)});
//
//   return tlas;
// }
//
// VkShaderModule RTEngine::loadShaderModule(const char *spv_path) {
//   auto code = ReadBinaryFile(spv_path);
//
//   VkShaderModuleCreateInfo ci{};
//   ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//   ci.codeSize = code.size();
//   ci.pCode = reinterpret_cast<const uint32_t *>(code.data());
//
//   VkShaderModule mod = VK_NULL_HANDLE;
//   VK_CHECK(vkCreateShaderModule(device_, &ci, nullptr, &mod));
//   return mod;
// }
//
// void RTEngine::createLSIDescriptorSetLayout() {
//   VkDescriptorSetLayoutBinding bindings[11]{};
//
//   // 0: acceleration structure
//   bindings[0].binding = 0;
//   bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//   bindings[0].descriptorCount = 1;
//   bindings[0].stageFlags =
//       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 1: params buffer (StructuredBuffer<LaunchParamsLSI>)
//   bindings[1].binding = 1;
//   bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[1].descriptorCount = 1;
//   bindings[1].stageFlags =
//       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 2: base edges
//   bindings[2].binding = 2;
//   bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[2].descriptorCount = 1;
//   bindings[2].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 3: base points
//   bindings[3].binding = 3;
//   bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[3].descriptorCount = 1;
//   bindings[3].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 4: eid ranges
//   bindings[4].binding = 4;
//   bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[4].descriptorCount = 1;
//   bindings[4].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 5: query edges
//   bindings[5].binding = 5;
//   bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[5].descriptorCount = 1;
//   bindings[5].stageFlags =
//       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 6: query points
//   bindings[6].binding = 6;
//   bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[6].descriptorCount = 1;
//   bindings[6].stageFlags =
//       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 7: xsect output
//   bindings[7].binding = 7;
//   bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[7].descriptorCount = 1;
//   bindings[7].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 8: xsect counter
//   bindings[8].binding = 8;
//   bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[8].descriptorCount = 1;
//   bindings[8].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 9: test/profile counter
//   bindings[9].binding = 9;
//   bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[9].descriptorCount = 1;
//   bindings[9].stageFlags =
//       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//
//   // 10: scaling buffer
//   bindings[10].binding = 10;
//   bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   bindings[10].descriptorCount = 1;
//   bindings[10].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
//
//   VkDescriptorSetLayoutCreateInfo ci{};
//   ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
//   ci.bindingCount = 11;
//   ci.pBindings = bindings;
//
//   VK_CHECK(vkCreateDescriptorSetLayout(device_, &ci, nullptr,
//                                        &lsi_desc_set_layout_));
// }
//
// void RTEngine::createLSIDescriptorPool() {
//   VkDescriptorPoolSize poolSizes[2]{};
//
//   poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//   poolSizes[0].descriptorCount = 1;
//
//   poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   poolSizes[1].descriptorCount = 10;
//
//   VkDescriptorPoolCreateInfo ci{};
//   ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
//   ci.maxSets = 1;
//   ci.poolSizeCount = 2;
//   ci.pPoolSizes = poolSizes;
//
//   VK_CHECK(vkCreateDescriptorPool(device_, &ci, nullptr, &lsi_desc_pool_));
// }
//
// void RTEngine::allocateLSIDescriptorSet() {
//   VkDescriptorSetAllocateInfo ai{};
//   ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//   ai.descriptorPool = lsi_desc_pool_;
//   ai.descriptorSetCount = 1;
//   ai.pSetLayouts = &lsi_desc_set_layout_;
//
//   VK_CHECK(vkAllocateDescriptorSets(device_, &ai, &lsi_desc_set_));
// }
//
// void RTEngine::createLSIPipelineLayout() {
//   VkPipelineLayoutCreateInfo ci{};
//   ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
//   ci.setLayoutCount = 1;
//   ci.pSetLayouts = &lsi_desc_set_layout_;
//
//   VK_CHECK(
//       vkCreatePipelineLayout(device_, &ci, nullptr, &lsi_pipeline_layout_));
// }
//
// void RTEngine::createLSIRTPipeline(const char *rgen_spv,
//                                    const char *rint_spv,
//                                    const char *rmiss_spv) {
//   (void)rmiss_spv;
//
//   VkShaderModule rgen = loadShaderModule(rgen_spv);
//   VkShaderModule rint = loadShaderModule(rint_spv);
//
//   VkPipelineShaderStageCreateInfo stages[2]{};
//
//   stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//   stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
//   stages[0].module = rgen;
//   stages[0].pName = "main";
//
//   stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//   stages[1].stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
//   stages[1].module = rint;
//   stages[1].pName = "main";
//
//   VkRayTracingShaderGroupCreateInfoKHR groups[2]{};
//
//   // raygen group
//   groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//   groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
//   groups[0].generalShader = 0;
//   groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
//   groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
//   groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;
//
//   // procedural hit group with intersection shader only
//   groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//   groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
//   groups[1].generalShader = VK_SHADER_UNUSED_KHR;
//   groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
//   groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
//   groups[1].intersectionShader = 1;
//
//   VkRayTracingPipelineCreateInfoKHR ci{};
//   ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
//   ci.stageCount = 2;
//   ci.pStages = stages;
//   ci.groupCount = 2;
//   ci.pGroups = groups;
//   ci.maxPipelineRayRecursionDepth = 1;
//   ci.layout = lsi_pipeline_layout_;
//
//   VK_CHECK(fpCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE,
//                                           VK_NULL_HANDLE, 1, &ci, nullptr,
//                                           &lsi_pipeline_));
//
//   vkDestroyShaderModule(device_, rgen, nullptr);
//   vkDestroyShaderModule(device_, rint, nullptr);
//
//   buildLSISBT(2);
// }
//
// void RTEngine::buildLSISBT(uint32_t group_count) {
//   VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
//   rtProps.sType =
//       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
//
//   VkPhysicalDeviceProperties2 props2{};
//   props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
//   props2.pNext = &rtProps;
//
//   vkGetPhysicalDeviceProperties2(ctx_->phys, &props2);
//
//   const uint32_t handleSize = rtProps.shaderGroupHandleSize;
//   const uint32_t handleAlign = rtProps.shaderGroupHandleAlignment;
//   const uint32_t baseAlign = rtProps.shaderGroupBaseAlignment;
//
//   const uint32_t handleSizeAligned = AlignUp(handleSize, handleAlign);
//   const uint32_t recordSize = AlignUp(handleSizeAligned, baseAlign);
//
//   lsi_sbt_stride_ = recordSize;
//
//   const uint32_t sbtSize = group_count * recordSize;
//
//   std::vector<unsigned char> handles(group_count * handleSize);
//   VK_CHECK(fpGetRayTracingShaderGroupHandlesKHR(
//       device_, lsi_pipeline_, 0, group_count,
//       static_cast<uint32_t>(handles.size()), handles.data()));
//
//   std::vector<unsigned char> sbt(sbtSize, 0);
//   for (uint32_t i = 0; i < group_count; ++i) {
//     std::memcpy(sbt.data() + i * recordSize,
//                 handles.data() + i * handleSize, handleSize);
//   }
//
//   lsi_sbt_buf_.InitSBT(sbtSize);
//
//   VkStagingBuf staging(sbtSize);
//   staging.Host2Stage(sbt);
//   staging.Stage2Device(lsi_sbt_buf_, sbtSize);
//
//   LOG(INFO) << "SBT props:"
//             << " handleSize=" << handleSize
//             << " handleAlign=" << handleAlign
//             << " baseAlign=" << baseAlign
//             << " handleSizeAligned=" << handleSizeAligned
//             << " recordSize=" << recordSize
//             << " sbtSize=" << sbtSize;
// }
//
// void RTEngine::InitLSIPipeline(const char *rgen_spv,
//                                const char *rint_spv,
//                                const char *rmiss_spv) {
//   createLSIDescriptorSetLayout();
//   createLSIDescriptorPool();
//   allocateLSIDescriptorSet();
//   createLSIPipelineLayout();
//   createLSIRTPipeline(rgen_spv, rint_spv, rmiss_spv);
//
//   if (lsi_desc_set_layout_ == VK_NULL_HANDLE) {
//     throw std::runtime_error(
//         "InitLSIPipeline(): lsi_desc_set_layout_ is null");
//   }
//   if (lsi_desc_pool_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_desc_pool_ is null");
//   }
//   if (lsi_desc_set_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_desc_set_ is null");
//   }
//   if (lsi_pipeline_layout_ == VK_NULL_HANDLE) {
//     throw std::runtime_error(
//         "InitLSIPipeline(): lsi_pipeline_layout_ is null");
//   }
//   if (lsi_pipeline_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("InitLSIPipeline(): lsi_pipeline_ is null");
//   }
// }
//
// void RTEngine::SetLSIQuery(VkAccelerationStructureKHR handle,
//                            const VkDeviceBuf &eid_range_buf,
//                            const VkDeviceBuf &base_points_buf,
//                            const VkDeviceBuf &base_edges_buf,
//                            const VkDeviceBuf &query_points_buf,
//                            const VkDeviceBuf &query_edges_buf,
//                            const VkDeviceBuf &scaling_buf,
//                            const VkDeviceBuf &xsect_buf,
//                            const VkDeviceBuf &xsect_counter_buf,
//                            const VkDeviceBuf &prof_counter_buf,
//                            uint32_t xsect_capacity,
//                            int query_map_id,
//                            uint32_t query_edge_count) {
//   if (device_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("SetLSIQuery(): device_ is null");
//   }
//   if (lsi_desc_set_ == VK_NULL_HANDLE) {
//     throw std::runtime_error(
//         "SetLSIQuery(): LSI pipeline/descriptors are not initialized. "
//         "Call InitLSIPipeline() before Query().");
//   }
//
//   lsi_query_.handle = handle;
//   lsi_query_.eid_range_buf = &eid_range_buf;
//   lsi_query_.base_points_buf = &base_points_buf;
//   lsi_query_.base_edges_buf = &base_edges_buf;
//   lsi_query_.query_points_buf = &query_points_buf;
//   lsi_query_.query_edges_buf = &query_edges_buf;
//   lsi_query_.scaling_buf = &scaling_buf;
//   lsi_query_.xsect_buf = &xsect_buf;
//   lsi_query_.xsect_counter_buf = &xsect_counter_buf;
//   lsi_query_.prof_counter_buf = &prof_counter_buf;
//   lsi_query_.xsect_capacity = xsect_capacity;
//   lsi_query_.query_map_id = query_map_id;
//   lsi_query_.query_edge_count = query_edge_count;
//
//   struct LaunchParamsLSI {
//     int query_map_id;
//     uint32_t query_edge_count;
//     uint32_t xsect_capacity;
//     uint32_t _pad0;
//   } params{query_map_id, query_edge_count, xsect_capacity, 0};
//
//   uploadLSIParams(params);
//   updateLSIDescriptors();
// }
//
// void RTEngine::updateLSIDescriptors() {
//   if (device_ == VK_NULL_HANDLE) {
//     throw std::runtime_error(
//         "RTEngine::updateLSIDescriptors(): device_ is null");
//   }
//   if (lsi_desc_set_ == VK_NULL_HANDLE) {
//     throw std::runtime_error(
//         "RTEngine::updateLSIDescriptors(): lsi_desc_set_ is null");
//   }
//   if (lsi_query_.handle == VK_NULL_HANDLE) {
//     throw std::runtime_error(
//         "RTEngine::updateLSIDescriptors(): lsi_query_.handle is null");
//   }
//   if (lsi_params_buf_.Buf() == VK_NULL_HANDLE) {
//     throw std::runtime_error(
//         "RTEngine::updateLSIDescriptors(): lsi_params_buf_ is null");
//   }
//   if (!lsi_query_.eid_range_buf || !lsi_query_.base_points_buf ||
//       !lsi_query_.base_edges_buf || !lsi_query_.query_points_buf ||
//       !lsi_query_.query_edges_buf || !lsi_query_.scaling_buf ||
//       !lsi_query_.xsect_buf || !lsi_query_.xsect_counter_buf ||
//       !lsi_query_.prof_counter_buf) {
//     throw std::runtime_error(
//         "RTEngine::updateLSIDescriptors(): one or more query buffers are null");
//   }
//
//   VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
//   asInfo.sType =
//       VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
//   asInfo.accelerationStructureCount = 1;
//   asInfo.pAccelerationStructures = &lsi_query_.handle;
//
//   VkDescriptorBufferInfo paramInfo{};
//   paramInfo.buffer = lsi_params_buf_.Buf();
//   paramInfo.offset = 0;
//   paramInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo baseEdgesInfo{};
//   baseEdgesInfo.buffer = lsi_query_.base_edges_buf->Buf();
//   baseEdgesInfo.offset = 0;
//   baseEdgesInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo basePointsInfo{};
//   basePointsInfo.buffer = lsi_query_.base_points_buf->Buf();
//   basePointsInfo.offset = 0;
//   basePointsInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo eidRangeInfo{};
//   eidRangeInfo.buffer = lsi_query_.eid_range_buf->Buf();
//   eidRangeInfo.offset = 0;
//   eidRangeInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo queryEdgesInfo{};
//   queryEdgesInfo.buffer = lsi_query_.query_edges_buf->Buf();
//   queryEdgesInfo.offset = 0;
//   queryEdgesInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo queryPointsInfo{};
//   queryPointsInfo.buffer = lsi_query_.query_points_buf->Buf();
//   queryPointsInfo.offset = 0;
//   queryPointsInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo xsectInfo{};
//   xsectInfo.buffer = lsi_query_.xsect_buf->Buf();
//   xsectInfo.offset = 0;
//   xsectInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo xsectCounterInfo{};
//   xsectCounterInfo.buffer = lsi_query_.xsect_counter_buf->Buf();
//   xsectCounterInfo.offset = 0;
//   xsectCounterInfo.range = sizeof(uint32_t);
//
//   VkDescriptorBufferInfo testCounterInfo{};
//   testCounterInfo.buffer = lsi_query_.prof_counter_buf->Buf();
//   testCounterInfo.offset = 0;
//   testCounterInfo.range = VK_WHOLE_SIZE;
//
//   VkDescriptorBufferInfo scalingInfo{};
//   scalingInfo.buffer = lsi_query_.scaling_buf->Buf();
//   scalingInfo.offset = 0;
//   scalingInfo.range = VK_WHOLE_SIZE;
//
//   VkWriteDescriptorSet writes[11]{};
//
//   // 0: AS
//   writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[0].pNext = &asInfo;
//   writes[0].dstSet = lsi_desc_set_;
//   writes[0].dstBinding = 0;
//   writes[0].descriptorCount = 1;
//   writes[0].descriptorType =
//       VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//
//   // 1: params
//   writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[1].dstSet = lsi_desc_set_;
//   writes[1].dstBinding = 1;
//   writes[1].descriptorCount = 1;
//   writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[1].pBufferInfo = &paramInfo;
//
//   // 2: base edges
//   writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[2].dstSet = lsi_desc_set_;
//   writes[2].dstBinding = 2;
//   writes[2].descriptorCount = 1;
//   writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[2].pBufferInfo = &baseEdgesInfo;
//
//   // 3: base points
//   writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[3].dstSet = lsi_desc_set_;
//   writes[3].dstBinding = 3;
//   writes[3].descriptorCount = 1;
//   writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[3].pBufferInfo = &basePointsInfo;
//
//   // 4: eid ranges
//   writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[4].dstSet = lsi_desc_set_;
//   writes[4].dstBinding = 4;
//   writes[4].descriptorCount = 1;
//   writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[4].pBufferInfo = &eidRangeInfo;
//
//   // 5: query edges
//   writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[5].dstSet = lsi_desc_set_;
//   writes[5].dstBinding = 5;
//   writes[5].descriptorCount = 1;
//   writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[5].pBufferInfo = &queryEdgesInfo;
//
//   // 6: query points
//   writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[6].dstSet = lsi_desc_set_;
//   writes[6].dstBinding = 6;
//   writes[6].descriptorCount = 1;
//   writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[6].pBufferInfo = &queryPointsInfo;
//
//   // 7: xsect output
//   writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[7].dstSet = lsi_desc_set_;
//   writes[7].dstBinding = 7;
//   writes[7].descriptorCount = 1;
//   writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[7].pBufferInfo = &xsectInfo;
//
//   // 8: xsect counter
//   writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[8].dstSet = lsi_desc_set_;
//   writes[8].dstBinding = 8;
//   writes[8].descriptorCount = 1;
//   writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[8].pBufferInfo = &xsectCounterInfo;
//
//   // 9: test/profile counter
//   writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[9].dstSet = lsi_desc_set_;
//   writes[9].dstBinding = 9;
//   writes[9].descriptorCount = 1;
//   writes[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[9].pBufferInfo = &testCounterInfo;
//
//   // 10: scaling
//   writes[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//   writes[10].dstSet = lsi_desc_set_;
//   writes[10].dstBinding = 10;
//   writes[10].descriptorCount = 1;
//   writes[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//   writes[10].pBufferInfo = &scalingInfo;
//
//   vkUpdateDescriptorSets(device_, 11, writes, 0, nullptr);
// }
//
// void RTEngine::RunLSI() {
//   if (lsi_pipeline_ == VK_NULL_HANDLE) {
//     throw std::runtime_error("RunLSI(): LSI pipeline not initialized");
//   }
//
//   if (lsi_query_.handle == VK_NULL_HANDLE) {
//     throw std::runtime_error("RunLSI(): no AS handle set");
//   }
//
//   VkCommandBuffer cmd = beginOneTime(device_, ctx_->cmdPool);
//
//   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
//                     lsi_pipeline_);
//   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
//                           lsi_pipeline_layout_, 0, 1, &lsi_desc_set_, 0,
//                           nullptr);
//
//   VkStridedDeviceAddressRegionKHR rgen{};
//   VkStridedDeviceAddressRegionKHR miss{};
//   VkStridedDeviceAddressRegionKHR hit{};
//   VkStridedDeviceAddressRegionKHR call{};
//
//   VkDeviceAddress sbtAddr = lsi_sbt_buf_.DeviceAddress();
//
//   rgen.deviceAddress = sbtAddr + 0 * lsi_sbt_stride_;
//   rgen.stride = lsi_sbt_stride_;
//   rgen.size = lsi_sbt_stride_;
//
//   hit.deviceAddress = sbtAddr + 1 * lsi_sbt_stride_;
//   hit.stride = lsi_sbt_stride_;
//   hit.size = lsi_sbt_stride_;
//
//   // No miss shader in this pipeline.
//   miss.deviceAddress = 0;
//   miss.stride = 0;
//   miss.size = 0;
//
//   call.deviceAddress = 0;
//   call.stride = 0;
//   call.size = 0;
//
//   LOG(INFO) << "SBT addrs:"
//             << " base=" << sbtAddr
//             << " rgen=" << rgen.deviceAddress
//             << " hit=" << hit.deviceAddress
//             << " miss=" << miss.deviceAddress
//             << " stride=" << lsi_sbt_stride_;
//
//   fpCmdTraceRaysKHR(cmd, &rgen, &miss, &hit, &call,
//                     lsi_query_.query_edge_count, 1, 1);
//
//   endSubmitWait(device_, ctx_->queue, ctx_->cmdPool, cmd);
// }
//
// } // namespace vk
// } // namespace rayjoin
