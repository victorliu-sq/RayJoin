#ifndef RAYJOIN_PRIMITIVE_NS_H
#define RAYJOIN_PRIMITIVE_NS_H

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_engine_abs.h"
#include "vk/engine/vk_helpers.h"


namespace rayjoin {
namespace vk {
class FillPrimitivesNS : public VkComputeEngine {
 public:
  FillPrimitivesNS(const char* spvPath,
                   const VkDeviceBuf& points,
                   const VkDeviceBuf& edges,
                   const VkDeviceBuf& aabbs,
                   const VkDeviceBuf& eidRange,
                   uint32_t numEdges,
                   uint32_t maxIter,
                   float areaEnlarge) :
      m_points(points), m_edges(edges), m_aabbs(aabbs), m_eidRange(eidRange), m_numEdges(numEdges), m_maxIter(maxIter), m_areaEnlarge(areaEnlarge) {
    createPipeline(spvPath);
    allocateDescriptors();
    recordDescriptors();
  }

 private:
  struct PushConstants {
    uint32_t numEdges;
    uint32_t maxIter;
    float areaEnlarge;
    uint32_t pad;
  };

  const VkDeviceBuf& m_points;
  const VkDeviceBuf& m_edges;
  const VkDeviceBuf& m_aabbs;
  const VkDeviceBuf& m_eidRange;

  uint32_t m_numEdges;
  uint32_t m_maxIter;
  float m_areaEnlarge;

  void createPipeline(const char* spvPath) override {
    VkDescriptorSetLayoutBinding bindings[4]{};

    for (uint32_t i = 0; i < 4; ++i) {
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
    dsl.bindingCount = 4;
    dsl.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &dsl, nullptr, &m_setLayout));

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstants);

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
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
    };

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets = 1;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = sizes;

    VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &ci, nullptr, &m_descPool));

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_ctx.device, &ai, &m_descSet));
  }

  void recordDescriptors() override {
    VkDescriptorBufferInfo pInfo{m_points.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo eInfo{m_edges.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo aInfo{m_aabbs.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo rInfo{m_eidRange.Buf(), 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[4]{};

    for (int i = 0; i < 4; ++i) {
      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wr[i].dstSet = m_descSet;
      wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      wr[i].descriptorCount = 1;
    }

    wr[0].dstBinding = 0;
    wr[0].pBufferInfo = &pInfo;

    wr[1].dstBinding = 1;
    wr[1].pBufferInfo = &eInfo;

    wr[2].dstBinding = 2;
    wr[2].pBufferInfo = &aInfo;

    wr[3].dstBinding = 3;
    wr[3].pBufferInfo = &rInfo;

    vkUpdateDescriptorSets(m_ctx.device, 4, wr, 0, nullptr);
  }

  void recordDispatch(VkCommandBuffer cmd) override {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

    PushConstants pc{m_numEdges, m_maxIter, m_areaEnlarge, 0};

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);

    const uint32_t groups = (m_numEdges + 63u) / 64u;
    vkCmdDispatch(cmd, groups, 1, 1);
  }
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_PRIMITIVE_NS_H
