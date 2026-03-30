#ifndef RAYJOIN_VK_RT_ENGINE_H
#define RAYJOIN_VK_RT_ENGINE_H
#include "vk_buffer.h"
#include "vk_engine_abs.h"


namespace rayjoin {
namespace vk {
template<typename ParamsT>
class VkRTEngine : public VkRTEngineBase {
 public:
  using params_t = ParamsT;

  template<typename... Buffers>
  VkRTEngine(const char* rgen_spv,
             const char* rint_spv,
             const char* rahit_spv,
             const char* rchit_spv,
             const char* rmiss_spv,
             VkAccelerationStructureKHR scene,
             const params_t& params,
             uint32_t traceWidth,
             const Buffers&... buffers) :
      m_scene(scene), m_params(params), m_traceWidth(traceWidth), m_buffers{std::cref(buffers)...}, m_rgenPath(rgen_spv), m_rintPath(rint_spv),
      m_rahitPath(rahit_spv), m_rchitPath(rchit_spv), m_rmissPath(rmiss_spv) {
    static_assert(std::is_trivially_copyable_v<params_t>, "ParamsT must be trivially copyable for vkCmdPushConstants");
    static_assert((std::is_same_v<std::remove_cvref_t<Buffers>, VkDeviceBuf> && ...), "All buffers must be VkDeviceBuf");

    if (m_buffers.empty()) {
      throw std::runtime_error("VkRTEngine requires at least one buffer");
    }

    loadFunctionPointers();
    createPipeline();
    allocateDescriptors();
    recordDescriptors();
    buildSBT();
  }

  ~VkRTEngine() override {
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

    groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;
    groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

    groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    groups[1].generalShader = VK_SHADER_UNUSED_KHR;
    groups[1].closestHitShader = 3;
    groups[1].anyHitShader = 2;
    groups[1].intersectionShader = 1;

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
      LOG(ERROR) << "vkCreateRayTracingPipelinesKHR failed, VkResult=" << res;
      throw std::runtime_error("vkCreateRayTracingPipelinesKHR failed");
    }
  }

  void allocateDescriptors() override {
    const uint32_t bufferCount = static_cast<uint32_t>(m_buffers.size());

    VkDescriptorPoolSize sizes[2]{};
    sizes[0] = {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1};
    sizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferCount};

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
    const uint32_t bufferCount = static_cast<uint32_t>(m_buffers.size());

    VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &m_scene;

    std::vector<VkDescriptorBufferInfo> infos(bufferCount);
    std::vector<VkWriteDescriptorSet> wr(bufferCount + 1);

    wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[0].pNext = &asInfo;
    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    wr[0].descriptorCount = 1;

    for (uint32_t i = 0; i < bufferCount; ++i) {
      infos[i] = VkDescriptorBufferInfo{m_buffers[i].get().Buf(), 0, VK_WHOLE_SIZE};

      wr[i + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wr[i + 1].dstSet = m_descSet;
      wr[i + 1].dstBinding = i + 1;
      wr[i + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      wr[i + 1].descriptorCount = 1;
      wr[i + 1].pBufferInfo = &infos[i];
    }

    vkUpdateDescriptorSets(m_ctx.device, static_cast<uint32_t>(wr.size()), wr.data(), 0, nullptr);
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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

    vkCmdPushConstants(cmd,
                       m_pipeLayout,
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                       0,
                       sizeof(params_t),
                       &m_params);

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

    fpCmdTraceRaysKHR(cmd, &rgen, &miss, &hit, &call, m_traceWidth, 1, 1);
  }

 private:
  void loadFunctionPointers() {
    fpCreateRayTracingPipelinesKHR =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkCreateRayTracingPipelinesKHR"));
    fpGetRayTracingShaderGroupHandlesKHR =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    fpCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_ctx.device, "vkCmdTraceRaysKHR"));

    if (!fpCreateRayTracingPipelinesKHR || !fpGetRayTracingShaderGroupHandlesKHR || !fpCmdTraceRaysKHR) {
      throw std::runtime_error("Missing Vulkan ray tracing entry points");
    }
  }

  void createDescriptorSetLayout() {
    const uint32_t bufferCount = static_cast<uint32_t>(m_buffers.size());

    std::vector<VkDescriptorSetLayoutBinding> bindings(bufferCount + 1);

    const VkShaderStageFlags hitStages = VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    bindings[0] = MakeBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | hitStages);

    for (uint32_t i = 0; i < bufferCount; ++i) {
      bindings[i + 1] =
          MakeBinding(i + 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | hitStages);
    }

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = static_cast<uint32_t>(bindings.size());
    ci.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &ci, nullptr, &m_setLayout));
  }

  void createPipelineLayout() {
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    pcr.offset = 0;
    pcr.size = sizeof(params_t);

    VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &m_setLayout;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges = &pcr;

    VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &ci, nullptr, &m_pipeLayout));
  }

  static uint32_t AlignUp(uint32_t x, uint32_t a) { return (x + a - 1u) & ~(a - 1u); }

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
  params_t m_params{};
  uint32_t m_traceWidth = 0;

  std::vector<std::reference_wrapper<const VkDeviceBuf>> m_buffers;

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

template<typename ParamsT, typename... Buffers>
void RunRTPass(const char* rgen_spv,
               const char* rint_spv,
               const char* rahit_spv,
               const char* rchit_spv,
               const char* rmiss_spv,
               VkAccelerationStructureKHR scene,
               const ParamsT& params,
               uint32_t traceWidth,
               const Buffers&... buffers) {
  VkRTEngine<ParamsT> pass(rgen_spv, rint_spv, rahit_spv, rchit_spv, rmiss_spv, scene, params, traceWidth, buffers...);
  pass.run();
}

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_VK_RT_ENGINE_H
