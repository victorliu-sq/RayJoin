#ifndef RAYJOIN_EDGE_INIT_PASS_I64_H
#define RAYJOIN_EDGE_INIT_PASS_I64_H

#include <string>
#include <vector>

#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_helpers.h"
#include "vk/map/gpu_edge_types.h"

class EdgeInitPassI64 {
 public:
  void init(const VkComputeContext& ctx, const char* spvPath) {
    m_ctx = ctx;

    // set layout: points, chains, row_index, edges
    VkDescriptorSetLayoutBinding b[4]{};

    b[0].binding = 0;
    b[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b[0].descriptorCount = 1;
    b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    b[1].binding = 1;
    b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b[1].descriptorCount = 1;
    b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    b[2].binding = 2;
    b[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b[2].descriptorCount = 1;
    b[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    b[3].binding = 3;
    b[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b[3].descriptorCount = 1;
    b[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dsl{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 4;
    dsl.pBindings = b;
    VK_CHECK(
        vkCreateDescriptorSetLayout(ctx.device, &dsl, nullptr, &m_setLayout));

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pl{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &pl, nullptr, &m_pipeLayout));

    auto spirv = readSpvU32(spvPath);
    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize = spirv.size() * sizeof(uint32_t);
    sm.pCode = spirv.data();
    VK_CHECK(vkCreateShaderModule(ctx.device, &sm, nullptr, &m_shader));

    VkPipelineShaderStageCreateInfo stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";

    VkComputePipelineCreateInfo cp{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cp.stage = stage;
    cp.layout = m_pipeLayout;
    VK_CHECK(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &cp,
                                      nullptr, &m_pipeline));

    // 1 set, 4 storage-buffer descriptors total.
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
    };

    VkDescriptorPoolCreateInfo ci{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.maxSets = 1;
    ci.poolSizeCount = (uint32_t) std::size(sizes);
    ci.pPoolSizes = sizes;
    // Optional: if you ever plan to free/re-allocate descriptor sets from this
    // pool: ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &ci, nullptr, &m_descPool));

    VkDescriptorSetAllocateInfo ai{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    // ai.descriptorPool = ctx.descPool;
    ai.descriptorPool = m_descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &ai, &m_descSet));

    m_inited = true;
  }

  void destroy() {
    if (!m_inited)
      return;
    cleanupBuffers();
    if (m_pipeline)
      vkDestroyPipeline(m_ctx.device, m_pipeline, nullptr);
    if (m_shader)
      vkDestroyShaderModule(m_ctx.device, m_shader, nullptr);
    if (m_pipeLayout)
      vkDestroyPipelineLayout(m_ctx.device, m_pipeLayout, nullptr);
    if (m_setLayout)
      vkDestroyDescriptorSetLayout(m_ctx.device, m_setLayout, nullptr);
    if (m_descPool)
      vkDestroyDescriptorPool(m_ctx.device, m_descPool, nullptr);
    *this = {};
  }

  void prepareBuffers(uint32_t numChains, uint32_t numPoints) {
    cleanupBuffers();

    m_numChains = numChains;
    m_numPoints = numPoints;
    m_numEdges = numPoints - numChains;

    // chains + row_index are uploaded from CPU
    VkDeviceSize chainsSize = sizeof(GpuChain) * numChains;
    VkDeviceSize rowSize = sizeof(GpuIndex) * (numChains + 1);
    VkDeviceSize edgesSize = sizeof(GpuEdge) * m_numEdges;

    m_chainsStaging = vmaCreateBufferSimple(m_ctx.vma, chainsSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);
    m_rowStaging = vmaCreateBufferSimple(m_ctx.vma, rowSize,
                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VMA_MEMORY_USAGE_CPU_ONLY);

    m_chainsDev = vmaCreateBufferSimple(
        m_ctx.vma, chainsSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    m_rowDev = vmaCreateBufferSimple(
        m_ctx.vma, rowSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    m_edgesDev = vmaCreateBufferSimple(
        m_ctx.vma, edgesSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // NOTE: points buffer comes from scale pass (external), set during run()

    m_ready = true;
  }

  // pointsDev must be the scaled points buffer (GpuPointI64) from the previous
  // pass
  void run(const AllocBuf& pointsDev, const std::vector<GpuChain>& chains,
           const std::vector<GpuIndex>& rowIndex) {
    if (!m_inited || !m_ready)
      throw std::runtime_error("EdgeInitPassI64 not ready");
    if ((uint32_t) chains.size() != m_numChains)
      throw std::runtime_error("chains size mismatch");
    if ((uint32_t) rowIndex.size() != m_numChains + 1)
      throw std::runtime_error("rowIndex size mismatch");

    // upload chains
    {
      void* mapped = nullptr;
      VK_CHECK(vmaMapMemory(m_ctx.vma, m_chainsStaging.alloc, &mapped));
      std::memcpy(mapped, chains.data(), sizeof(GpuChain) * m_numChains);
      vmaUnmapMemory(m_ctx.vma, m_chainsStaging.alloc);
    }
    // upload rowIndex
    {
      void* mapped = nullptr;
      VK_CHECK(vmaMapMemory(m_ctx.vma, m_rowStaging.alloc, &mapped));
      std::memcpy(mapped, rowIndex.data(),
                  sizeof(GpuIndex) * (m_numChains + 1));
      vmaUnmapMemory(m_ctx.vma, m_rowStaging.alloc);
    }

    VkCommandBuffer cmd = beginOneTime(m_ctx.device, m_ctx.cmdPool);

    // staging -> device copies
    VkBufferCopy c0{0, 0, sizeof(GpuChain) * m_numChains};
    vkCmdCopyBuffer(cmd, m_chainsStaging.buf, m_chainsDev.buf, 1, &c0);

    VkBufferCopy c1{0, 0, sizeof(GpuIndex) * (m_numChains + 1)};
    vkCmdCopyBuffer(cmd, m_rowStaging.buf, m_rowDev.buf, 1, &c1);

    // Barrier: transfer -> compute for chains/row
    // VkBufferMemoryBarrier2 bars[3]{};
    // bars[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    // bars[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    // bars[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    // bars[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    // bars[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    // bars[0].buffer = m_chainsDev.buf;
    // bars[0].size = VK_WHOLE_SIZE;
    //
    // bars[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    // bars[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    // bars[1].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    // bars[1].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    // bars[1].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    // bars[1].buffer = m_rowDev.buf;
    // bars[1].size = VK_WHOLE_SIZE;
    //
    // bars[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    // bars[2].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    // bars[2].srcAccessMask = 0;
    // bars[2].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    // bars[2].dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    // bars[2].buffer = m_edgesDev.buf;
    // bars[2].size = VK_WHOLE_SIZE;
    //
    // VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    // dep.bufferMemoryBarrierCount = 3;
    // dep.pBufferMemoryBarriers = bars;
    // vkCmdPipelineBarrier2(cmd, &dep);

    // VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    // dep.bufferMemoryBarrierCount = 3;
    // dep.pBufferMemoryBarriers = bars;
    // vkCmdPipelineBarrier2(cmd, &dep);

    // With only 2 barriers
    VkBufferMemoryBarrier2 bars[2]{};
    bars[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bars[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    bars[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bars[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bars[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    bars[0].buffer = m_chainsDev.buf;
    bars[0].size = VK_WHOLE_SIZE;

    bars[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bars[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    bars[1].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bars[1].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bars[1].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    bars[1].buffer = m_rowDev.buf;
    bars[1].size = VK_WHOLE_SIZE;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.bufferMemoryBarrierCount = 2;
    dep.pBufferMemoryBarriers = bars;
    vkCmdPipelineBarrier2(cmd, &dep);

    // update descriptors (points is external, others are internal)
    VkDescriptorBufferInfo pInfo{pointsDev.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo cInfo{m_chainsDev.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo rInfo{m_rowDev.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo eInfo{m_edgesDev.buf, 0, VK_WHOLE_SIZE};

    // update descriptorSet
    VkWriteDescriptorSet wr[4]{};
    for (int i = 0; i < 4; ++i) {
      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    }

    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[0].descriptorCount = 1;
    wr[0].pBufferInfo = &pInfo;
    wr[1].dstSet = m_descSet;
    wr[1].dstBinding = 1;
    wr[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[1].descriptorCount = 1;
    wr[1].pBufferInfo = &cInfo;
    wr[2].dstSet = m_descSet;
    wr[2].dstBinding = 2;
    wr[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[2].descriptorCount = 1;
    wr[2].pBufferInfo = &rInfo;
    wr[3].dstSet = m_descSet;
    wr[3].dstBinding = 3;
    wr[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[3].descriptorCount = 1;
    wr[3].pBufferInfo = &eInfo;

    vkUpdateDescriptorSets(m_ctx.device, 4, wr, 0, nullptr);

    // bind + push + dispatch
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout,
                            0, 1, &m_descSet, 0, nullptr);

    PushConstants pc;
    pc.numPoints = m_numPoints;
    pc.numChains = m_numChains;
    pc.numEdges = m_numEdges;

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstants), &pc);

    uint32_t groups = (m_numPoints + 256 - 1) / 256;
    vkCmdDispatch(cmd, groups, 1, 1);

    // make edges visible for later passes / readback
    VkBufferMemoryBarrier2 after{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    after.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    after.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    after.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    after.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    after.buffer = m_edgesDev.buf;
    after.size = VK_WHOLE_SIZE;

    VkDependencyInfo dep2{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep2.bufferMemoryBarrierCount = 1;
    dep2.pBufferMemoryBarriers = &after;
    vkCmdPipelineBarrier2(cmd, &dep2);

    endSubmitWait(m_ctx.device, m_ctx.queue, m_ctx.cmdPool, cmd);
  }

  const AllocBuf& edgesBuffer() const { return m_edgesDev; }
  uint32_t numEdges() const { return m_numEdges; }

 private:
  void cleanupBuffers() {
    vmaDestroyBufferSafe(m_ctx.vma, m_chainsStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_rowStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_chainsDev);
    vmaDestroyBufferSafe(m_ctx.vma, m_rowDev);
    vmaDestroyBufferSafe(m_ctx.vma, m_edgesDev);
    m_ready = false;
  }

  struct PushConstants {
    uint32_t numPoints, numChains, numEdges, pad;
  };

  VkComputeContext m_ctx{};
  bool m_inited = false;
  bool m_ready = false;

  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
  VkShaderModule m_shader = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
  VkDescriptorSet m_descSet = VK_NULL_HANDLE;
  VkDescriptorPool m_descPool = VK_NULL_HANDLE;

  uint32_t m_numPoints = 0;
  uint32_t m_numChains = 0;
  uint32_t m_numEdges = 0;

  AllocBuf m_chainsStaging{}, m_rowStaging{};
  AllocBuf m_chainsDev{}, m_rowDev{};
  AllocBuf m_edgesDev{};
};

#endif  // RAYJOIN_EDGE_INIT_PASS_I64_H
