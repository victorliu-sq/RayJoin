#ifndef RAYJOIN_PRIMITIVE_NS_H
#define RAYJOIN_PRIMITIVE_NS_H

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_engine_abs.h"
#include "vk/engine/vk_helpers.h"

namespace rayjoin {
namespace vk {

template<typename ParamsT>
class VkComputeEngine : public VkComputeEngineBase {
 public:
  using params_t = ParamsT;

  template<typename... Buffers>
  VkComputeEngine(uint32_t n, const char* spvPath, const params_t& params, const Buffers&... buffers) :
      m_n(n), m_params(params), m_buffers{std::cref(buffers)...} {
    static_assert(std::is_trivially_copyable_v<params_t>, "ParamsT must be trivially copyable for vkCmdPushConstants");
    static_assert((std::is_same_v<std::remove_cvref_t<Buffers>, VkDeviceBuf> && ...), "All buffers must be VkDeviceBuf");

    if (m_buffers.empty()) {
      throw std::runtime_error("VkComputeEngine requires at least one buffer");
    }

    createPipeline(spvPath);
    allocateDescriptors();
    recordDescriptors();
  }

  void setParams(const params_t& params) { m_params = params; }
  void setDispatchSize(uint32_t n) { m_n = n; }

  void record(VkCommandBuffer cmd) { recordDispatch(cmd); }

 private:
  uint32_t m_n;
  params_t m_params;
  std::vector<std::reference_wrapper<const VkDeviceBuf>> m_buffers;

  void createPipeline(const char* spvPath) override {
    const uint32_t bufferCount = static_cast<uint32_t>(m_buffers.size());

    std::vector<VkDescriptorSetLayoutBinding> bindings(bufferCount);
    for (uint32_t i = 0; i < bufferCount; ++i) {
      bindings[i].binding = i;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo dsl{};
    dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl.pNext = nullptr;
    dsl.flags = 0;
    dsl.bindingCount = bufferCount;
    dsl.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &dsl, nullptr, &m_setLayout));

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(params_t);

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pNext = nullptr;
    pl.flags = 0;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;

    VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &pl, nullptr, &m_pipeLayout));

    auto spirv = readSpvU32(spvPath);
    if (spirv.empty()) {
      throw std::runtime_error(std::string("Failed to load SPIR-V: ") + spvPath);
    }

    VkShaderModuleCreateInfo sm{};
    sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sm.pNext = nullptr;
    sm.flags = 0;
    sm.codeSize = spirv.size() * sizeof(uint32_t);
    sm.pCode = spirv.data();

    VK_CHECK(vkCreateShaderModule(m_ctx.device, &sm, nullptr, &m_shader));

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.pNext = nullptr;
    stage.flags = 0;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";
    stage.pSpecializationInfo = nullptr;

    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp.pNext = nullptr;
    cp.flags = 0;
    cp.stage = stage;
    cp.layout = m_pipeLayout;
    cp.basePipelineHandle = VK_NULL_HANDLE;
    cp.basePipelineIndex = -1;

    VK_CHECK(vkCreateComputePipelines(m_ctx.device, VK_NULL_HANDLE, 1, &cp, nullptr, &m_pipeline));
  }

  void allocateDescriptors() override {
    const uint32_t bufferCount = static_cast<uint32_t>(m_buffers.size());

    VkDescriptorPoolSize size{};
    size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    size.descriptorCount = bufferCount;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets = 1;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &size;

    VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &ci, nullptr, &m_descPool));

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_ctx.device, &ai, &m_descSet));
  }

  void recordDescriptors() override {
    const uint32_t bufferCount = static_cast<uint32_t>(m_buffers.size());

    std::vector<VkDescriptorBufferInfo> infos(bufferCount);
    std::vector<VkWriteDescriptorSet> wr(bufferCount);

    for (uint32_t i = 0; i < bufferCount; ++i) {
      infos[i] = VkDescriptorBufferInfo{m_buffers[i].get().Buf(), 0, VK_WHOLE_SIZE};

      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wr[i].pNext = nullptr;
      wr[i].dstSet = m_descSet;
      wr[i].dstBinding = i;
      wr[i].dstArrayElement = 0;
      wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      wr[i].descriptorCount = 1;
      wr[i].pBufferInfo = &infos[i];
      wr[i].pImageInfo = nullptr;
      wr[i].pTexelBufferView = nullptr;
    }

    vkUpdateDescriptorSets(m_ctx.device, bufferCount, wr.data(), 0, nullptr);
  }

  void recordDispatch(VkCommandBuffer cmd) override {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(params_t), &m_params);

    const uint32_t groups = (m_n + 63u) / 64u;
    vkCmdDispatch(cmd, groups, 1, 1);
  }
};


template<typename ParamsT, typename... Buffers>
void RunComputePass(uint32_t n, const char* spvPath, const ParamsT& params, const Buffers&... buffers) {
  VkComputeEngine<ParamsT> pass(n, spvPath, params, buffers...);
  pass.run();
}
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_PRIMITIVE_NS_H
