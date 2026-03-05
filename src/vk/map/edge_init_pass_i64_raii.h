#ifndef RAYJOIN_EDGE_INIT_PASS_I64_RAII_H
#define RAYJOIN_EDGE_INIT_PASS_I64_RAII_H

#include <glog/logging.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_engine_abs.h"
#include "vk/engine/vk_helpers.h"
#include "vk/map/gpu_edge_types.h"

namespace rayjoin {
namespace vk {

class EdgeInitPassI64RAII : public VkComputeEngine {
 public:
  EdgeInitPassI64RAII(const VkComputeContext& ctx, const char* spvPath,
                      const AllocBuf& pointsDev,
                      const std::vector<GpuChain>& chains,
                      const std::vector<GpuIndex>& rowIndex)
      : VkComputeEngine(ctx, spvPath),
        m_pointsDev(pointsDev),
        m_chainsCPU(chains),
        m_rowCPU(rowIndex) {
    if (chains.empty())
      throw std::runtime_error("EdgeInitPass: chains empty");

    if (rowIndex.size() < 2)
      throw std::runtime_error("EdgeInitPass: rowIndex invalid");

    m_numChains = (uint32_t) chains.size();
    m_numPoints = (uint32_t) rowIndex.back();
    m_numEdges = m_numPoints - m_numChains;

    LOG(INFO) << "[EdgeInitPass] chains=" << m_numChains
              << " points=" << m_numPoints << " edges=" << m_numEdges;

    LOG(INFO) << "[EdgeInitPass] sizeof(GpuEdge)=" << sizeof(GpuEdge);

    setup(spvPath);
  }

  ~EdgeInitPassI64RAII() {
    // cleanupBuffers();
    vmaDestroyBufferSafe(m_ctx.vma, m_chainsStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_rowStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_chainsDev);
    vmaDestroyBufferSafe(m_ctx.vma, m_rowDev);
    vmaDestroyBufferSafe(m_ctx.vma, m_edgesDev);
  }

  const AllocBuf& edgesBuffer() const { return m_edgesDev; }

 private:
  struct PushConstants {
    uint32_t numPoints;
    uint32_t numChains;
    uint32_t numEdges;
    uint32_t pad;
  };

  /* ---------------- Pipeline ---------------- */
  void createPipeline(const char* spvPath) override {
    LOG(INFO) << "[EdgeInitPass] loading shader: " << spvPath;

    auto spirv = readSpvU32(spvPath);

    LOG(INFO) << "[EdgeInitPass] SPIR-V size = "
              << spirv.size() * sizeof(uint32_t) << " bytes";

    VkDescriptorSetLayoutBinding b[4]{};
    for (int i = 0; i < 4; ++i) {
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

  /* ---------------- Descriptors ---------------- */

  void allocateDescriptors() override {
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
    };

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

  /* ---------------- Buffers ---------------- */

  void createBuffers() override {
    VkDeviceSize chainsSize = sizeof(GpuChain) * m_numChains;
    VkDeviceSize rowSize = sizeof(GpuIndex) * (m_numChains + 1);
    VkDeviceSize edgesSize = sizeof(GpuEdge) * m_numEdges;

    LOG(INFO) << "[EdgeInitPass] edges buffer size = " << edgesSize << " bytes";

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

    m_edgesDev = vmaCreateBufferSimple(m_ctx.vma, edgesSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VMA_MEMORY_USAGE_GPU_ONLY);
  }

  /* ---------------- Upload CPU Data ---------------- */

  void uploadCPUData() override {
    void* mapped;

    VK_CHECK(vmaMapMemory(m_ctx.vma, m_chainsStaging.alloc, &mapped));

    memcpy(mapped, m_chainsCPU.data(), sizeof(GpuChain) * m_numChains);

    vmaUnmapMemory(m_ctx.vma, m_chainsStaging.alloc);

    VK_CHECK(vmaMapMemory(m_ctx.vma, m_rowStaging.alloc, &mapped));

    memcpy(mapped, m_rowCPU.data(), sizeof(GpuIndex) * (m_numChains + 1));

    vmaUnmapMemory(m_ctx.vma, m_rowStaging.alloc);
  }

  /* ---------------- Copy Commands ---------------- */

  void recordCopy(VkCommandBuffer cmd) override {
    VkBufferCopy c0{0, 0, sizeof(GpuChain) * m_numChains};
    vkCmdCopyBuffer(cmd, m_chainsStaging.buf, m_chainsDev.buf, 1, &c0);

    VkBufferCopy c1{0, 0, sizeof(GpuIndex) * (m_numChains + 1)};
    vkCmdCopyBuffer(cmd, m_rowStaging.buf, m_rowDev.buf, 1, &c1);
  }

  void recordBarrier(VkCommandBuffer cmd) override {
    VkBufferMemoryBarrier2 bars[2]{};

    for (int i = 0; i < 2; i++) {
      bars[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
      bars[i].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      bars[i].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      bars[i].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      bars[i].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      bars[i].size = VK_WHOLE_SIZE;
    }

    bars[0].buffer = m_chainsDev.buf;
    bars[1].buffer = m_rowDev.buf;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.bufferMemoryBarrierCount = 2;
    dep.pBufferMemoryBarriers = bars;

    vkCmdPipelineBarrier2(cmd, &dep);
  }

  void preDispatch(VkCommandBuffer cmd) override {
    vkCmdFillBuffer(cmd, m_edgesDev.buf, 0, VK_WHOLE_SIZE, 0);
  }

  /* ---------------- Descriptor Writes ---------------- */

  void recordDescriptors() override {
    VkDescriptorBufferInfo pInfo{m_pointsDev.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo cInfo{m_chainsDev.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo rInfo{m_rowDev.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo eInfo{m_edgesDev.buf, 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[4]{};

    for (int i = 0; i < 4; i++)
      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[0].descriptorCount = 1;
    wr[0].pBufferInfo = &pInfo;

    wr[1] = wr[0];
    wr[1].dstBinding = 1;
    wr[1].pBufferInfo = &cInfo;
    wr[2] = wr[0];
    wr[2].dstBinding = 2;
    wr[2].pBufferInfo = &rInfo;
    wr[3] = wr[0];
    wr[3].dstBinding = 3;
    wr[3].pBufferInfo = &eInfo;

    vkUpdateDescriptorSets(m_ctx.device, 4, wr, 0, nullptr);
  }

  /* ---------------- Dispatch ---------------- */

  void recordDispatch(VkCommandBuffer cmd) override {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout,
                            0, 1, &m_descSet, 0, nullptr);

    PushConstants pc{m_numPoints, m_numChains, m_numEdges, 0};

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(pc), &pc);

    uint32_t groups = (m_numPoints + 255) / 256;

    LOG(INFO) << "[EdgeInitPass] dispatch groups = " << groups;

    vkCmdDispatch(cmd, groups, 1, 1);
  }

  void recordPostBarrier(VkCommandBuffer cmd) override {
    VkBufferMemoryBarrier2 after{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};

    after.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    after.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    after.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    after.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    after.buffer = m_edgesDev.buf;
    after.size = VK_WHOLE_SIZE;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.bufferMemoryBarrierCount = 1;
    dep.pBufferMemoryBarriers = &after;

    vkCmdPipelineBarrier2(cmd, &dep);
  }

 private:
  uint32_t m_numPoints{};
  uint32_t m_numChains{};
  uint32_t m_numEdges{};

  AllocBuf m_pointsDev{};

  std::vector<GpuChain> m_chainsCPU;
  std::vector<GpuIndex> m_rowCPU;

  AllocBuf m_chainsStaging{}, m_rowStaging{};
  AllocBuf m_chainsDev{}, m_rowDev{};
  AllocBuf m_edgesDev{};
};

}  // namespace vk
}  // namespace rayjoin
#endif