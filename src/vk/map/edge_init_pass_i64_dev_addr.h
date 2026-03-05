#ifndef RAYJOIN_VK_EDGE_INIT_DEV_ADDR
#define RAYJOIN_VK_EDGE_INIT_DEV_ADDR

#include <string>
#include <vector>

#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_helpers.h"
#include "vk/map/gpu_edge_types.h"

class EdgeInitPassI64DevAddr {
 public:
  void init(const VkComputeContext& ctx, const char* spvPath) {
    m_ctx = ctx;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pl{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
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

    *this = {};
  }

  void prepareBuffers(uint32_t numChains, uint32_t numPoints) {
    cleanupBuffers();

    m_numChains = numChains;
    m_numPoints = numPoints;
    m_numEdges = numPoints - numChains;

    VkDeviceSize chainsSize = sizeof(GpuChain) * numChains;
    VkDeviceSize rowSize = sizeof(GpuIndex) * (numChains + 1);
    VkDeviceSize edgesSize = sizeof(GpuEdge) * m_numEdges;

    m_chainsStaging = vmaCreateBufferSimple(m_ctx.vma, chainsSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);

    m_rowStaging = vmaCreateBufferSimple(m_ctx.vma, rowSize,
                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VMA_MEMORY_USAGE_CPU_ONLY);

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    m_chainsDev = vmaCreateBufferSimple(m_ctx.vma, chainsSize, usage,
                                        VMA_MEMORY_USAGE_GPU_ONLY);

    m_rowDev = vmaCreateBufferSimple(m_ctx.vma, rowSize, usage,
                                     VMA_MEMORY_USAGE_GPU_ONLY);

    m_edgesDev = vmaCreateBufferSimple(
        m_ctx.vma, edgesSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    m_ready = true;
  }

  void run(const AllocBuf& pointsDev, const std::vector<GpuChain>& chains,
           const std::vector<GpuIndex>& rowIndex) {
    if (!m_inited || !m_ready)
      throw std::runtime_error("EdgeInitPassI64 not ready");

    if ((uint32_t) chains.size() != m_numChains)
      throw std::runtime_error("chains size mismatch");

    if ((uint32_t) rowIndex.size() != m_numChains + 1)
      throw std::runtime_error("rowIndex size mismatch");

    // Upload chains
    {
      void* mapped = nullptr;
      VK_CHECK(vmaMapMemory(m_ctx.vma, m_chainsStaging.alloc, &mapped));
      std::memcpy(mapped, chains.data(), sizeof(GpuChain) * m_numChains);
      vmaUnmapMemory(m_ctx.vma, m_chainsStaging.alloc);
    }

    // Upload rowIndex
    {
      void* mapped = nullptr;
      VK_CHECK(vmaMapMemory(m_ctx.vma, m_rowStaging.alloc, &mapped));
      std::memcpy(mapped, rowIndex.data(),
                  sizeof(GpuIndex) * (m_numChains + 1));
      vmaUnmapMemory(m_ctx.vma, m_rowStaging.alloc);
    }

    VkCommandBuffer cmd = beginOneTime(m_ctx.device, m_ctx.cmdPool);

    // Copy staging → device
    VkBufferCopy c0{0, 0, sizeof(GpuChain) * m_numChains};
    vkCmdCopyBuffer(cmd, m_chainsStaging.buf, m_chainsDev.buf, 1, &c0);

    VkBufferCopy c1{0, 0, sizeof(GpuIndex) * (m_numChains + 1)};
    vkCmdCopyBuffer(cmd, m_rowStaging.buf, m_rowDev.buf, 1, &c1);

    // Transfer → compute barrier
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

    // ---- Device addresses ----
    uint64_t pointsAddr = getAddr(pointsDev.buf);
    uint64_t chainsAddr = getAddr(m_chainsDev.buf);
    uint64_t rowAddr = getAddr(m_rowDev.buf);
    uint64_t edgesAddr = getAddr(m_edgesDev.buf);

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    PushConstants pc{};
    pc.pointsAddr = pointsAddr;
    pc.chainsAddr = chainsAddr;
    pc.rowIdxAddr = rowAddr;
    pc.edgesAddr = edgesAddr;

    pc.numPoints = m_numPoints;
    pc.numChains = m_numChains;
    pc.numEdges = m_numEdges;

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstants), &pc);

    uint32_t groups = (m_numPoints + 256 - 1) / 256;

    vkCmdDispatch(cmd, groups, 1, 1);

    // Compute → read barrier
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
  struct PushConstants {
    uint64_t pointsAddr;
    uint64_t chainsAddr;
    uint64_t rowIdxAddr;
    uint64_t edgesAddr;

    uint32_t numPoints;
    uint32_t numChains;
    uint32_t numEdges;
    uint32_t pad;
  };

  static_assert(sizeof(PushConstants) <= 128);

  uint64_t getAddr(VkBuffer buf) const {
    VkBufferDeviceAddressInfo info{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    info.buffer = buf;
    return vkGetBufferDeviceAddress(m_ctx.device, &info);
  }

  void cleanupBuffers() {
    vmaDestroyBufferSafe(m_ctx.vma, m_chainsStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_rowStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_chainsDev);
    vmaDestroyBufferSafe(m_ctx.vma, m_rowDev);
    vmaDestroyBufferSafe(m_ctx.vma, m_edgesDev);
    m_ready = false;
  }

  VkComputeContext m_ctx{};
  bool m_inited = false;
  bool m_ready = false;

  VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
  VkShaderModule m_shader = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;

  uint32_t m_numPoints = 0;
  uint32_t m_numChains = 0;
  uint32_t m_numEdges = 0;

  AllocBuf m_chainsStaging{}, m_rowStaging{};
  AllocBuf m_chainsDev{}, m_rowDev{};
  AllocBuf m_edgesDev{};
};

#endif  // RAYJOIN_VK_EDGE_INIT_DEV_ADDR
