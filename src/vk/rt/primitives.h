#ifndef RAYJOIN_PRIMITIVES_H
#define RAYJOIN_PRIMITIVES_H

#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_engine_abs.h"
#include "vk/engine/vk_helpers.h"

namespace rayjoin {
namespace vk {

class FillPrimitivesGroupNewPass : public VkComputeEngine {
 public:
  FillPrimitivesGroupNewPass(const char* spvPath, const AllocBuf& points,
                             const AllocBuf& edges, const AllocBuf& aabbs,
                             const AllocBuf& eidRange, uint32_t numEdges,
                             uint32_t maxIter, float areaEnlarge)
      : VkComputeEngine(),
        m_points(points),
        m_edges(edges),
        m_aabbs(aabbs),
        m_eidRange(eidRange),
        m_numEdges(numEdges),
        m_maxIter(maxIter),
        m_areaEnlarge(areaEnlarge) {
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

  uint32_t m_numEdges;
  uint32_t m_maxIter;
  float m_areaEnlarge;

  AllocBuf m_points;
  AllocBuf m_edges;
  AllocBuf m_aabbs;
  AllocBuf m_eidRange;

  void createPipeline(const char* spvPath) override {
    VkDescriptorSetLayoutBinding b[4]{};

    for (int i = 0; i < 4; i++) {
      b[i].binding = i;
      b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      b[i].descriptorCount = 1;
      b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 4;
    dsl.pBindings = b;

    VK_CHECK(
        vkCreateDescriptorSetLayout(m_ctx.device, &dsl, nullptr, &m_setLayout));

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pl{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;

    VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &pl, nullptr, &m_pipeLayout));

    auto spirv = readSpvU32(spvPath);

    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize = spirv.size() * sizeof(uint32_t);
    sm.pCode = spirv.data();

    VK_CHECK(vkCreateShaderModule(m_ctx.device, &sm, nullptr, &m_shader));

    VkPipelineShaderStageCreateInfo stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";

    VkComputePipelineCreateInfo cp{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cp.stage = stage;
    cp.layout = m_pipeLayout;

    VK_CHECK(vkCreateComputePipelines(m_ctx.device, VK_NULL_HANDLE, 1, &cp,
                                      nullptr, &m_pipeline));
  }

  void allocateDescriptors() override {
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}};

    VkDescriptorPoolCreateInfo ci{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.maxSets = 1;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = sizes;

    VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &ci, nullptr, &m_descPool));

    VkDescriptorSetAllocateInfo ai{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_ctx.device, &ai, &m_descSet));
  }

  void recordDescriptors() override {
    VkDescriptorBufferInfo pInfo{m_points.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo eInfo{m_edges.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo aInfo{m_aabbs.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo rInfo{m_eidRange.buf, 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[4]{};

    for (int i = 0; i < 4; i++)
      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    wr[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[0].descriptorCount = 1;
    wr[0].pBufferInfo = &pInfo;

    wr[1] = wr[0];
    wr[1].dstBinding = 1;
    wr[1].pBufferInfo = &eInfo;
    wr[2] = wr[0];
    wr[2].dstBinding = 2;
    wr[2].pBufferInfo = &aInfo;
    wr[3] = wr[0];
    wr[3].dstBinding = 3;
    wr[3].pBufferInfo = &rInfo;

    vkUpdateDescriptorSets(m_ctx.device, 4, wr, 0, nullptr);
  }

  void recordDispatch(VkCommandBuffer cmd) override {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout,
                            0, 1, &m_descSet, 0, nullptr);

    PushConstants pc{m_numEdges, m_maxIter, m_areaEnlarge, 0};

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(pc), &pc);

    uint32_t groups = (m_numEdges + 63) / 64;

    vkCmdDispatch(cmd, groups, 1, 1);
  }
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_PRIMITIVES_H
