#ifndef RAYJOIN_LSI_FINALIZE_PASS_H
#define RAYJOIN_LSI_FINALIZE_PASS_H

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_engine_abs.h"

namespace rayjoin {
namespace vk {

class LSIFinalizePassRAII : public VkComputeEngine {
 public:
  LSIFinalizePassRAII(const char* spvPath,
                      const VkDeviceBuf& paramsDev,
                      const VkDeviceBuf& baseEdgesDev,
                      const VkDeviceBuf& basePointsDev,
                      const VkDeviceBuf& queryEdgesDev,
                      const VkDeviceBuf& queryPointsDev,
                      const VkDeviceBuf& xsectsDev,
                      const VkDeviceBuf& xsectCounterDev,
                      uint32_t xsectCapacity) :
      VkComputeEngine(), m_paramsDev(paramsDev), m_baseEdgesDev(baseEdgesDev), m_basePointsDev(basePointsDev), m_queryEdgesDev(queryEdgesDev),
      m_queryPointsDev(queryPointsDev), m_xsectsDev(xsectsDev), m_xsectCounterDev(xsectCounterDev), m_xsectCapacity(xsectCapacity) {
    createPipeline(spvPath);
    allocateDescriptors();
    recordDescriptors();
  }

 protected:
  void createPipeline(const char* spvPath) override {
    VkDescriptorSetLayoutBinding b[7]{};

    for (uint32_t i = 0; i < 7; ++i) {
      b[i].binding = i;
      b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      b[i].descriptorCount = 1;
      b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl{};
    dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl.bindingCount = 7;
    dsl.pBindings = b;

    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &dsl, nullptr, &m_setLayout));

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;

    VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &pl, nullptr, &m_pipeLayout));

    auto spirv = readSpvU32(spvPath);

    VkShaderModuleCreateInfo sm{};
    sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sm.codeSize = spirv.size() * sizeof(uint32_t);
    sm.pCode = spirv.data();

    VK_CHECK(vkCreateShaderModule(m_ctx.device, &sm, nullptr, &m_shader));

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";

    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp.stage = stage;
    cp.layout = m_pipeLayout;

    VK_CHECK(vkCreateComputePipelines(m_ctx.device, VK_NULL_HANDLE, 1, &cp, nullptr, &m_pipeline));
  }

  void allocateDescriptors() override {
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7},
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
    VkDescriptorBufferInfo paramsInfo{m_paramsDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo baseEdgesInfo{m_baseEdgesDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo basePointsInfo{m_basePointsDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo queryEdgesInfo{m_queryEdgesDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo queryPointsInfo{m_queryPointsDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo xsectsInfo{m_xsectsDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo xsectCounterInfo{m_xsectCounterDev.Buf(), 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[7]{};
    for (int i = 0; i < 7; ++i) {
      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wr[i].dstSet = m_descSet;
      wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      wr[i].descriptorCount = 1;
      wr[i].dstBinding = i;
    }

    wr[0].pBufferInfo = &paramsInfo;
    wr[1].pBufferInfo = &baseEdgesInfo;
    wr[2].pBufferInfo = &basePointsInfo;
    wr[3].pBufferInfo = &queryEdgesInfo;
    wr[4].pBufferInfo = &queryPointsInfo;
    wr[5].pBufferInfo = &xsectsInfo;
    wr[6].pBufferInfo = &xsectCounterInfo;

    vkUpdateDescriptorSets(m_ctx.device, 7, wr, 0, nullptr);
  }

  void recordDispatch(VkCommandBuffer cmd) override {
    // Make RT shader writes visible to this compute shader.
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

    uint32_t groups = (m_xsectCapacity + 255u) / 256u;
    vkCmdDispatch(cmd, groups, 1, 1);
  }

 private:
  const VkDeviceBuf& m_paramsDev;
  const VkDeviceBuf& m_baseEdgesDev;
  const VkDeviceBuf& m_basePointsDev;
  const VkDeviceBuf& m_queryEdgesDev;
  const VkDeviceBuf& m_queryPointsDev;
  const VkDeviceBuf& m_xsectsDev;
  const VkDeviceBuf& m_xsectCounterDev;
  uint32_t m_xsectCapacity{};
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_LSI_FINALIZE_PASS_H
