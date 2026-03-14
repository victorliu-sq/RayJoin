#ifndef RAYJOIN_VK_EDGE_INIT_PASS_RAII_NS_H
#define RAYJOIN_VK_EDGE_INIT_PASS_RAII_NS_H

#include <string>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_engine_abs.h"

#ifndef SHADER_DIR_NS
#error "SHADER_DIR_NS is not defined. Pass it from CMake via target_compile_definitions(...)."
#endif

namespace rayjoin {
namespace vk {

class EdgeInitPassRAIINS : public VkComputeEngine {
 public:
  EdgeInitPassRAIINS(const VkDeviceBuf& pointsDev,
                     const VkDeviceBuf& chainsDev,
                     const VkDeviceBuf& rowDev,
                     const VkDeviceBuf& edgesDev,
                     uint32_t numPoints,
                     uint32_t numChains) :
      VkComputeEngine(), m_pointsDev(pointsDev), m_chainsDev(chainsDev), m_rowDev(rowDev), m_edgesDev(edgesDev), m_numPoints(numPoints),
      m_numChains(numChains) {
    m_numEdges = m_numPoints - m_numChains;
    createPipeline();
    allocateDescriptors();
    recordDescriptors();
  }

  const VkDeviceBuf& edgesBuffer() const { return m_edgesDev; }

 protected:
  struct PushConstants {
    uint32_t numPoints;
    uint32_t numChains;
    uint32_t numEdges;
    uint32_t pad;
  };

  static std::string GetShaderPath() { return std::string(SHADER_DIR_NS) + "/edge_init_ns_d64.spv"; }

  void createPipeline() {
    const std::string spvPath = GetShaderPath();

    VkDescriptorSetLayoutBinding b[4]{};

    for (int i = 0; i < 4; ++i) {
      b[i].binding = i;
      b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      b[i].descriptorCount = 1;
      b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 4;
    dsl.pBindings = b;

    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &dsl, nullptr, &m_setLayout));

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;

    VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &pl, nullptr, &m_pipeLayout));

    auto spirv = readSpvU32(spvPath.c_str());

    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize = spirv.size() * sizeof(uint32_t);
    sm.pCode = spirv.data();

    VK_CHECK(vkCreateShaderModule(m_ctx.device, &sm, nullptr, &m_shader));

    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";

    VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cp.stage = stage;
    cp.layout = m_pipeLayout;

    VK_CHECK(vkCreateComputePipelines(m_ctx.device, VK_NULL_HANDLE, 1, &cp, nullptr, &m_pipeline));
  }

  void createPipeline(const char* spvPath) override {
    (void) spvPath;
    createPipeline();
  }

  void allocateDescriptors() override {
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
    };

    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.maxSets = 1;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = sizes;

    VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &ci, nullptr, &m_descPool));

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_ctx.device, &ai, &m_descSet));
  }

  void recordDescriptors() override {
    VkDescriptorBufferInfo pInfo{m_pointsDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo cInfo{m_chainsDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo rInfo{m_rowDev.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo eInfo{m_edgesDev.Buf(), 0, VK_WHOLE_SIZE};

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
    wr[1].pBufferInfo = &cInfo;

    wr[2].dstBinding = 2;
    wr[2].pBufferInfo = &rInfo;

    wr[3].dstBinding = 3;
    wr[3].pBufferInfo = &eInfo;

    vkUpdateDescriptorSets(m_ctx.device, 4, wr, 0, nullptr);
  }

  void recordDispatch(VkCommandBuffer cmd) override {
    vkCmdFillBuffer(cmd, m_edgesDev.Buf(), 0, VK_WHOLE_SIZE, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

    PushConstants pc{m_numPoints, m_numChains, m_numEdges, 0};

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    uint32_t groups = (m_numPoints + 255) / 256;
    vkCmdDispatch(cmd, groups, 1, 1);
  }

 private:
  uint32_t m_numPoints{};
  uint32_t m_numChains{};
  uint32_t m_numEdges{};

  const VkDeviceBuf& m_pointsDev;
  const VkDeviceBuf& m_chainsDev;
  const VkDeviceBuf& m_rowDev;
  const VkDeviceBuf& m_edgesDev;
};

}  // namespace vk
}  // namespace rayjoin

#endif
