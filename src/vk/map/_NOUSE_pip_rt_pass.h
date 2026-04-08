#ifndef RAYJOIN_PIP_RT_PASS_H
#define RAYJOIN_PIP_RT_PASS_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_engine_abs.h"

// class PIPRTPassNS : public rayjoin::vk::VkRayTracingEngine {
//  public:
//   struct LaunchParamsPIP {
//     int32_t query_map_id;
//     uint32_t query_point_count;
//     uint32_t _pad0;
//     uint32_t _pad1;
//   };
//
//   PIPRTPassNS(const char* rgen_spv,
//               const char* rint_spv,
//               const char* rahit_spv,
//               const char* rchit_spv,
//               const char* rmiss_spv,
//               VkAccelerationStructureKHR scene,
//               const VkDeviceBuf& eid_range_buf,
//               const VkDeviceBuf& base_points_buf,
//               const VkDeviceBuf& base_edges_buf,
//               const VkDeviceBuf& query_points_buf,
//               const VkDeviceBuf& closest_eids_buf,
//               const VkDeviceBuf& debug_counter_buf,
//               uint32_t query_map_id,
//               uint32_t query_point_count) :
//       m_scene(scene), m_eidRangeBuf(eid_range_buf), m_basePointsBuf(base_points_buf), m_baseEdgesBuf(base_edges_buf),
//       m_queryPointsBuf(query_points_buf), m_closestEidsBuf(closest_eids_buf), m_debugCounterBuf(debug_counter_buf),
//       m_params{static_cast<int32_t>(query_map_id), query_point_count, 0u, 0u}, m_rgenPath(rgen_spv), m_rintPath(rint_spv), m_rahitPath(rahit_spv),
//       m_rchitPath(rchit_spv), m_rmissPath(rmiss_spv) {
//     loadFunctionPointers();
//     createPipeline();
//     allocateDescriptors();
//     uploadParams();
//     recordDescriptors();
//     buildSBT();
//   }
//
//   ~PIPRTPassNS() override {
//     if (m_rgen) vkDestroyShaderModule(m_ctx.device, m_rgen, nullptr);
//     if (m_rint) vkDestroyShaderModule(m_ctx.device, m_rint, nullptr);
//     if (m_rahit) vkDestroyShaderModule(m_ctx.device, m_rahit, nullptr);
//     if (m_rchit) vkDestroyShaderModule(m_ctx.device, m_rchit, nullptr);
//     if (m_rmiss) vkDestroyShaderModule(m_ctx.device, m_rmiss, nullptr);
//   }
//
//  protected:
//   void createPipeline() override {
//     createDescriptorSetLayout();
//     createPipelineLayout();
//
//     m_rgen = loadShaderModule(m_rgenPath.c_str());
//     m_rint = loadShaderModule(m_rintPath.c_str());
//     m_rahit = loadShaderModule(m_rahitPath.c_str());
//     m_rchit = loadShaderModule(m_rchitPath.c_str());
//     m_rmiss = loadShaderModule(m_rmissPath.c_str());
//
//     VkPipelineShaderStageCreateInfo stages[5]{};
//     stages[0] = MakeStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, m_rgen, "main");
//     stages[1] = MakeStage(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, m_rint, "main");
//     stages[2] = MakeStage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, m_rahit, "main");
//     stages[3] = MakeStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, m_rchit, "main");
//     stages[4] = MakeStage(VK_SHADER_STAGE_MISS_BIT_KHR, m_rmiss, "main");
//
//     VkRayTracingShaderGroupCreateInfoKHR groups[3]{};
//
//     // group 0: raygen
//     groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//     groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
//     groups[0].generalShader = 0;
//     groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
//     groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
//     groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;
//
//     // group 1: procedural hit group
//     groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//     groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
//     groups[1].generalShader = VK_SHADER_UNUSED_KHR;
//     groups[1].closestHitShader = 3;
//     groups[1].anyHitShader = 2;
//     groups[1].intersectionShader = 1;
//
//     // group 2: miss
//     groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
//     groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
//     groups[2].generalShader = 4;
//     groups[2].closestHitShader = VK_SHADER_UNUSED_KHR;
//     groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
//     groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;
//
//     VkRayTracingPipelineCreateInfoKHR ci{};
//     ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
//     ci.stageCount = 5;
//     ci.pStages = stages;
//     ci.groupCount = 3;
//     ci.pGroups = groups;
//     ci.maxPipelineRayRecursionDepth = 1;
//     ci.layout = m_pipeLayout;
//
//     VkResult res = fpCreateRayTracingPipelinesKHR(m_ctx.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipeline);
//
//     if (res != VK_SUCCESS) {
//       throw std::runtime_error("PIPRTPassNS: vkCreateRayTracingPipelinesKHR failed");
//     }
//   }
//
//   void allocateDescriptors() override {
//     VkDescriptorPoolSize sizes[2]{};
//     sizes[0] = {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1};
//     sizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7};
//
//     VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
//     pi.maxSets = 1;
//     pi.poolSizeCount = 2;
//     pi.pPoolSizes = sizes;
//
//     VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &pi, nullptr, &m_descPool));
//
//     VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
//     ai.descriptorPool = m_descPool;
//     ai.descriptorSetCount = 1;
//     ai.pSetLayouts = &m_setLayout;
//
//     VK_CHECK(vkAllocateDescriptorSets(m_ctx.device, &ai, &m_descSet));
//   }
//
//   void recordDescriptors() override {
//     VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
//     asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
//     asInfo.accelerationStructureCount = 1;
//     asInfo.pAccelerationStructures = &m_scene;
//
//     VkDescriptorBufferInfo paramsInfo{m_paramsBuf.Buf(), 0, sizeof(LaunchParamsPIP)};
//     VkDescriptorBufferInfo baseEdgesInfo{m_baseEdgesBuf.Buf(), 0, VK_WHOLE_SIZE};
//     VkDescriptorBufferInfo basePointsInfo{m_basePointsBuf.Buf(), 0, VK_WHOLE_SIZE};
//     VkDescriptorBufferInfo eidRangeInfo{m_eidRangeBuf.Buf(), 0, VK_WHOLE_SIZE};
//     VkDescriptorBufferInfo queryPointsInfo{m_queryPointsBuf.Buf(), 0, VK_WHOLE_SIZE};
//     VkDescriptorBufferInfo closestEidsInfo{m_closestEidsBuf.Buf(), 0, VK_WHOLE_SIZE};
//     VkDescriptorBufferInfo debugCounterInfo{m_debugCounterBuf.Buf(), 0, VK_WHOLE_SIZE};
//
//     VkWriteDescriptorSet wr[8]{};
//
//     wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     wr[0].pNext = &asInfo;
//     wr[0].dstSet = m_descSet;
//     wr[0].dstBinding = 0;
//     wr[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//     wr[0].descriptorCount = 1;
//
//     auto set_sb = [&](int i, uint32_t binding, VkDescriptorBufferInfo* info) {
//       wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//       wr[i].dstSet = m_descSet;
//       wr[i].dstBinding = binding;
//       wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//       wr[i].descriptorCount = 1;
//       wr[i].pBufferInfo = info;
//     };
//
//     set_sb(1, 1, &paramsInfo);
//     set_sb(2, 2, &baseEdgesInfo);
//     set_sb(3, 3, &basePointsInfo);
//     set_sb(4, 4, &eidRangeInfo);
//     set_sb(5, 5, &queryPointsInfo);
//     set_sb(6, 6, &closestEidsInfo);
//     set_sb(7, 7, &debugCounterInfo);
//
//     vkUpdateDescriptorSets(m_ctx.device, 8, wr, 0, nullptr);
//   }
//
//   void buildSBT() override {
//     VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
//     rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
//
//     VkPhysicalDeviceProperties2 props2{};
//     props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
//     props2.pNext = &rtProps;
//
//     vkGetPhysicalDeviceProperties2(m_ctx.phys, &props2);
//
//     const uint32_t handleSize = rtProps.shaderGroupHandleSize;
//     const uint32_t handleAlign = rtProps.shaderGroupHandleAlignment;
//     const uint32_t baseAlign = rtProps.shaderGroupBaseAlignment;
//
//     const uint32_t handleSizeAligned = AlignUp(handleSize, handleAlign);
//     m_sbtStride = AlignUp(handleSizeAligned, baseAlign);
//
//     const uint32_t groupCount = 3;
//     const uint32_t sbtSize = groupCount * m_sbtStride;
//
//     std::vector<uint8_t> handles(groupCount * handleSize);
//     VK_CHECK(fpGetRayTracingShaderGroupHandlesKHR(m_ctx.device, m_pipeline, 0, groupCount, static_cast<uint32_t>(handles.size()), handles.data()));
//
//     std::vector<uint8_t> sbt(sbtSize, 0);
//     for (uint32_t i = 0; i < groupCount; ++i) {
//       std::memcpy(sbt.data() + i * m_sbtStride, handles.data() + i * handleSize, handleSize);
//     }
//
//     m_sbtBuf.InitSBT(sbtSize);
//
//     VkStagingBuf staging(sbtSize);
//     staging.Host2Stage(sbt);
//     staging.Stage2Device(m_sbtBuf, sbtSize);
//   }
//
//   void recordTrace(VkCommandBuffer cmd) override {
//     // closest_eids := UINT32_MAX
//     vkCmdFillBuffer(cmd, m_closestEidsBuf.Buf(), 0, VK_WHOLE_SIZE, 0xFFFFFFFFu);
//
//     // debug counters := 0
//     vkCmdFillBuffer(cmd, m_debugCounterBuf.Buf(), 0, VK_WHOLE_SIZE, 0u);
//
//     VkMemoryBarrier fillBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
//     fillBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//     fillBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
//
//     vkCmdPipelineBarrier(
//         cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &fillBarrier, 0, nullptr, 0, nullptr);
//
//     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
//     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);
//
//     VkDeviceAddress sbtAddr = m_sbtBuf.DeviceAddress();
//
//     VkStridedDeviceAddressRegionKHR rgen{};
//     rgen.deviceAddress = sbtAddr + 0 * m_sbtStride;
//     rgen.stride = m_sbtStride;
//     rgen.size = m_sbtStride;
//
//     VkStridedDeviceAddressRegionKHR hit{};
//     hit.deviceAddress = sbtAddr + 1 * m_sbtStride;
//     hit.stride = m_sbtStride;
//     hit.size = m_sbtStride;
//
//     VkStridedDeviceAddressRegionKHR miss{};
//     miss.deviceAddress = sbtAddr + 2 * m_sbtStride;
//     miss.stride = m_sbtStride;
//     miss.size = m_sbtStride;
//
//     VkStridedDeviceAddressRegionKHR call{};
//
//     fpCmdTraceRaysKHR(cmd, &rgen, &miss, &hit, &call, m_params.query_point_count, 1, 1);
//   }
//
//  private:
//   static uint32_t AlignUp(uint32_t x, uint32_t a) { return (x + a - 1u) & ~(a - 1u); }
//
//   void loadFunctionPointers() {
//     fpCreateRayTracingPipelinesKHR =
//         reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkCreateRayTracingPipelinesKHR"));
//
//     fpGetRayTracingShaderGroupHandlesKHR =
//         reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkGetRayTracingShaderGroupHandlesKHR"));
//
//     fpCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkCmdTraceRaysKHR"));
//
//     if (!fpCreateRayTracingPipelinesKHR || !fpGetRayTracingShaderGroupHandlesKHR || !fpCmdTraceRaysKHR) {
//       throw std::runtime_error("PIPRTPassNS: missing Vulkan ray tracing entry points");
//     }
//   }
//
//   void createDescriptorSetLayout() {
//     VkDescriptorSetLayoutBinding bindings[8]{};
//
//     const VkShaderStageFlags hitStages = VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
//     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
//
//     bindings[0] = MakeBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);
//
//     bindings[1] = MakeBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);
//
//     bindings[2] = MakeBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, hitStages);
//
//     bindings[3] = MakeBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, hitStages);
//
//     bindings[4] = MakeBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, hitStages);
//
//     bindings[5] = MakeBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);
//
//     bindings[6] = MakeBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);
//
//     bindings[7] = MakeBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | hitStages);
//
//     VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
//     ci.bindingCount = 8;
//     ci.pBindings = bindings;
//
//     VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &ci, nullptr, &m_setLayout));
//   }
//
//   void createPipelineLayout() {
//     VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
//     ci.setLayoutCount = 1;
//     ci.pSetLayouts = &m_setLayout;
//
//     VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &ci, nullptr, &m_pipeLayout));
//   }
//
//   void uploadParams() {
//     m_paramsBuf.Init(sizeof(LaunchParamsPIP));
//
//     VkStagingBuf staging(sizeof(LaunchParamsPIP));
//     std::vector<uint8_t> bytes(sizeof(LaunchParamsPIP));
//     std::memcpy(bytes.data(), &m_params, sizeof(LaunchParamsPIP));
//
//     staging.Host2Stage(bytes);
//     staging.Stage2Device(m_paramsBuf, sizeof(LaunchParamsPIP));
//   }
//
//   static VkDescriptorSetLayoutBinding MakeBinding(uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages) {
//     VkDescriptorSetLayoutBinding b{};
//     b.binding = binding;
//     b.descriptorType = type;
//     b.descriptorCount = count;
//     b.stageFlags = stages;
//     return b;
//   }
//
//   static VkPipelineShaderStageCreateInfo MakeStage(VkShaderStageFlagBits stage, VkShaderModule module, const char* entry) {
//     VkPipelineShaderStageCreateInfo s{};
//     s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//     s.stage = stage;
//     s.module = module;
//     s.pName = entry;
//     return s;
//   }
//
//   VkShaderModule loadShaderModule(const char* path) {
//     auto spirv = readSpvU32(path);
//
//     VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
//     ci.codeSize = spirv.size() * sizeof(uint32_t);
//     ci.pCode = spirv.data();
//
//     VkShaderModule mod = VK_NULL_HANDLE;
//     VK_CHECK(vkCreateShaderModule(m_ctx.device, &ci, nullptr, &mod));
//     return mod;
//   }
//
//  private:
//   VkAccelerationStructureKHR m_scene = VK_NULL_HANDLE;
//
//   const VkDeviceBuf& m_eidRangeBuf;
//   const VkDeviceBuf& m_basePointsBuf;
//   const VkDeviceBuf& m_baseEdgesBuf;
//   const VkDeviceBuf& m_queryPointsBuf;
//   const VkDeviceBuf& m_closestEidsBuf;
//   const VkDeviceBuf& m_debugCounterBuf;
//
//   LaunchParamsPIP m_params{};
//   VkDeviceBuf m_paramsBuf{};
//
//   std::string m_rgenPath;
//   std::string m_rintPath;
//   std::string m_rahitPath;
//   std::string m_rchitPath;
//   std::string m_rmissPath;
//
//   VkShaderModule m_rgen = VK_NULL_HANDLE;
//   VkShaderModule m_rint = VK_NULL_HANDLE;
//   VkShaderModule m_rahit = VK_NULL_HANDLE;
//   VkShaderModule m_rchit = VK_NULL_HANDLE;
//   VkShaderModule m_rmiss = VK_NULL_HANDLE;
// };

class PIPRTPassNS : public rayjoin::vk::VkRTEngineBase {
 public:
  struct LaunchParamsPIP {
    int32_t query_map_id;
    uint32_t query_point_count;
    uint32_t _pad0;
    uint32_t _pad1;
  };

  PIPRTPassNS(const char* rgen_spv,
              const char* rint_spv,
              const char* rahit_spv,
              const char* rchit_spv,
              const char* rmiss_spv,
              VkAccelerationStructureKHR scene,
              const VkDeviceBuf& eid_range_buf,
              const VkDeviceBuf& base_points_buf,
              const VkDeviceBuf& base_edges_buf,
              const VkDeviceBuf& query_points_buf,
              const VkDeviceBuf& closest_eids_buf,
              const VkDeviceBuf& best_ys_buf,
              const VkDeviceBuf& debug_counter_buf,
              uint32_t query_map_id,
              uint32_t query_point_count) :
      m_scene(scene), m_eidRangeBuf(eid_range_buf), m_basePointsBuf(base_points_buf), m_baseEdgesBuf(base_edges_buf),
      m_queryPointsBuf(query_points_buf), m_closestEidsBuf(closest_eids_buf), m_bestYsBuf(best_ys_buf), m_debugCounterBuf(debug_counter_buf),
      m_params{static_cast<int32_t>(query_map_id), query_point_count, 0u, 0u}, m_rgenPath(rgen_spv), m_rintPath(rint_spv), m_rahitPath(rahit_spv),
      m_rchitPath(rchit_spv), m_rmissPath(rmiss_spv) {
    loadFunctionPointers();
    createPipeline();
    allocateDescriptors();
    uploadParams();
    recordDescriptors();
    buildSBT();
  }

  ~PIPRTPassNS() override {
    if (m_rgen) vkDestroyShaderModule(m_ctx.device, m_rgen, nullptr);
    if (m_rint) vkDestroyShaderModule(m_ctx.device, m_rint, nullptr);
    if (m_rahit) vkDestroyShaderModule(m_ctx.device, m_rahit, nullptr);
    if (m_rchit) vkDestroyShaderModule(m_ctx.device, m_rchit, nullptr);
    if (m_rmiss) vkDestroyShaderModule(m_ctx.device, m_rmiss, nullptr);
  }

 protected:
  void createPipeline() override {
    createDescriptorSetLayout();
    createPipelineLayout();

    m_rgen = loadShaderModule(m_rgenPath.c_str());
    m_rint = loadShaderModule(m_rintPath.c_str());
    m_rahit = loadShaderModule(m_rahitPath.c_str());
    m_rchit = loadShaderModule(m_rchitPath.c_str());
    m_rmiss = loadShaderModule(m_rmissPath.c_str());

    VkPipelineShaderStageCreateInfo stages[5]{};
    stages[0] = MakeStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, m_rgen, "main");
    stages[1] = MakeStage(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, m_rint, "main");
    stages[2] = MakeStage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, m_rahit, "main");
    stages[3] = MakeStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, m_rchit, "main");
    stages[4] = MakeStage(VK_SHADER_STAGE_MISS_BIT_KHR, m_rmiss, "main");

    VkRayTracingShaderGroupCreateInfoKHR groups[3]{};

    // group 0: raygen
    groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;
    groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

    // group 1: procedural hit group
    groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    groups[1].generalShader = VK_SHADER_UNUSED_KHR;
    groups[1].closestHitShader = 3;
    groups[1].anyHitShader = 2;
    groups[1].intersectionShader = 1;

    // group 2: miss
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
    ci.layout = m_pipeLayout;

    VkResult res = fpCreateRayTracingPipelinesKHR(m_ctx.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipeline);

    if (res != VK_SUCCESS) {
      throw std::runtime_error("PIPRTPassNS: vkCreateRayTracingPipelinesKHR failed");
    }
  }

  void allocateDescriptors() override {
    VkDescriptorPoolSize sizes[2]{};
    sizes[0] = {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1};
    sizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9};

    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = 1;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = sizes;

    VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &pi, nullptr, &m_descPool));

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_ctx.device, &ai, &m_descSet));
  }

  void recordDescriptors() override {
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &m_scene;

    VkDescriptorBufferInfo paramsInfo{m_paramsBuf.Buf(), 0, sizeof(LaunchParamsPIP)};
    VkDescriptorBufferInfo baseEdgesInfo{m_baseEdgesBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo basePointsInfo{m_basePointsBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo eidRangeInfo{m_eidRangeBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo queryPointsInfo{m_queryPointsBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo closestEidsInfo{m_closestEidsBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo bestYsInfo{m_bestYsBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo debugCounterInfo{m_debugCounterBuf.Buf(), 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[9]{};

    wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[0].pNext = &asInfo;
    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    wr[0].descriptorCount = 1;

    auto set_sb = [&](int i, uint32_t binding, VkDescriptorBufferInfo* info) {
      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wr[i].dstSet = m_descSet;
      wr[i].dstBinding = binding;
      wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      wr[i].descriptorCount = 1;
      wr[i].pBufferInfo = info;
    };

    set_sb(1, 1, &paramsInfo);
    set_sb(2, 2, &baseEdgesInfo);
    set_sb(3, 3, &basePointsInfo);
    set_sb(4, 4, &eidRangeInfo);
    set_sb(5, 5, &queryPointsInfo);
    set_sb(6, 6, &closestEidsInfo);
    set_sb(7, 7, &bestYsInfo);
    set_sb(8, 8, &debugCounterInfo);

    vkUpdateDescriptorSets(m_ctx.device, 9, wr, 0, nullptr);
  }

  void buildSBT() override {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;

    vkGetPhysicalDeviceProperties2(m_ctx.phys, &props2);

    const uint32_t handleSize = rtProps.shaderGroupHandleSize;
    const uint32_t handleAlign = rtProps.shaderGroupHandleAlignment;
    const uint32_t baseAlign = rtProps.shaderGroupBaseAlignment;

    const uint32_t handleSizeAligned = AlignUp(handleSize, handleAlign);
    m_sbtStride = AlignUp(handleSizeAligned, baseAlign);

    const uint32_t groupCount = 3;
    const uint32_t sbtSize = groupCount * m_sbtStride;

    std::vector<uint8_t> handles(groupCount * handleSize);
    VK_CHECK(fpGetRayTracingShaderGroupHandlesKHR(m_ctx.device, m_pipeline, 0, groupCount, static_cast<uint32_t>(handles.size()), handles.data()));

    std::vector<uint8_t> sbt(sbtSize, 0);
    for (uint32_t i = 0; i < groupCount; ++i) {
      std::memcpy(sbt.data() + i * m_sbtStride, handles.data() + i * handleSize, handleSize);
    }

    m_sbtBuf.InitSBT(sbtSize);

    VkStagingBuf staging(sbtSize);
    staging.Host2Stage(sbt);
    staging.Stage2Device(m_sbtBuf, sbtSize);
  }

  void recordTrace(VkCommandBuffer cmd) override {
    // closest_eids := UINT32_MAX
    vkCmdFillBuffer(cmd, m_closestEidsBuf.Buf(), 0, VK_WHOLE_SIZE, 0xFFFFFFFFu);

    // best_ys := +inf encoded as double 1.0e300
    {
      const size_t elem_count = m_params.query_point_count;
      std::vector<double> host_best_ys(elem_count, 1.0e300);
      VkStagingBuf staging(sizeof(double) * elem_count);
      staging.Host2Stage(host_best_ys);
      staging.Stage2Device(m_bestYsBuf, sizeof(double) * elem_count);
    }

    // debug counters := 0
    vkCmdFillBuffer(cmd, m_debugCounterBuf.Buf(), 0, VK_WHOLE_SIZE, 0u);

    VkMemoryBarrier fillBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    fillBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    fillBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &fillBarrier, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

    VkDeviceAddress sbtAddr = m_sbtBuf.DeviceAddress();

    VkStridedDeviceAddressRegionKHR rgen{};
    rgen.deviceAddress = sbtAddr + 0 * m_sbtStride;
    rgen.stride = m_sbtStride;
    rgen.size = m_sbtStride;

    VkStridedDeviceAddressRegionKHR hit{};
    hit.deviceAddress = sbtAddr + 1 * m_sbtStride;
    hit.stride = m_sbtStride;
    hit.size = m_sbtStride;

    VkStridedDeviceAddressRegionKHR miss{};
    miss.deviceAddress = sbtAddr + 2 * m_sbtStride;
    miss.stride = m_sbtStride;
    miss.size = m_sbtStride;

    VkStridedDeviceAddressRegionKHR call{};

    fpCmdTraceRaysKHR(cmd, &rgen, &miss, &hit, &call, m_params.query_point_count, 1, 1);
  }

 private:
  static uint32_t AlignUp(uint32_t x, uint32_t a) { return (x + a - 1u) & ~(a - 1u); }

  void loadFunctionPointers() {
    fpCreateRayTracingPipelinesKHR =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkCreateRayTracingPipelinesKHR"));

    fpGetRayTracingShaderGroupHandlesKHR =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkGetRayTracingShaderGroupHandlesKHR"));

    fpCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkCmdTraceRaysKHR"));

    if (!fpCreateRayTracingPipelinesKHR || !fpGetRayTracingShaderGroupHandlesKHR || !fpCmdTraceRaysKHR) {
      throw std::runtime_error("PIPRTPassNS: missing Vulkan ray tracing entry points");
    }
  }

  void createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding bindings[9]{};

    const VkShaderStageFlags hitStages = VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    bindings[0] = MakeBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);

    bindings[1] = MakeBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);

    bindings[2] = MakeBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, hitStages);

    bindings[3] = MakeBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, hitStages);

    bindings[4] = MakeBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, hitStages);

    bindings[5] = MakeBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);

    bindings[6] = MakeBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);

    bindings[7] = MakeBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);

    bindings[8] = MakeBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | hitStages);

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 9;
    ci.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &ci, nullptr, &m_setLayout));
  }

  void createPipelineLayout() {
    VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &m_setLayout;

    VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &ci, nullptr, &m_pipeLayout));
  }

  void uploadParams() {
    m_paramsBuf.Init(sizeof(LaunchParamsPIP));

    VkStagingBuf staging(sizeof(LaunchParamsPIP));
    std::vector<uint8_t> bytes(sizeof(LaunchParamsPIP));
    std::memcpy(bytes.data(), &m_params, sizeof(LaunchParamsPIP));

    staging.Host2Stage(bytes);
    staging.Stage2Device(m_paramsBuf, sizeof(LaunchParamsPIP));
  }

  static VkDescriptorSetLayoutBinding MakeBinding(uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = count;
    b.stageFlags = stages;
    return b;
  }

  static VkPipelineShaderStageCreateInfo MakeStage(VkShaderStageFlagBits stage, VkShaderModule module, const char* entry) {
    VkPipelineShaderStageCreateInfo s{};
    s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    s.stage = stage;
    s.module = module;
    s.pName = entry;
    return s;
  }

  VkShaderModule loadShaderModule(const char* path) {
    auto spirv = readSpvU32(path);

    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode = spirv.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(m_ctx.device, &ci, nullptr, &mod));
    return mod;
  }

 private:
  VkAccelerationStructureKHR m_scene = VK_NULL_HANDLE;

  const VkDeviceBuf& m_eidRangeBuf;
  const VkDeviceBuf& m_basePointsBuf;
  const VkDeviceBuf& m_baseEdgesBuf;
  const VkDeviceBuf& m_queryPointsBuf;
  const VkDeviceBuf& m_closestEidsBuf;
  const VkDeviceBuf& m_bestYsBuf;
  const VkDeviceBuf& m_debugCounterBuf;

  LaunchParamsPIP m_params{};
  VkDeviceBuf m_paramsBuf{};

  std::string m_rgenPath;
  std::string m_rintPath;
  std::string m_rahitPath;
  std::string m_rchitPath;
  std::string m_rmissPath;

  VkShaderModule m_rgen = VK_NULL_HANDLE;
  VkShaderModule m_rint = VK_NULL_HANDLE;
  VkShaderModule m_rahit = VK_NULL_HANDLE;
  VkShaderModule m_rchit = VK_NULL_HANDLE;
  VkShaderModule m_rmiss = VK_NULL_HANDLE;
};

#endif  // RAYJOIN_PIP_RT_PASS_H
