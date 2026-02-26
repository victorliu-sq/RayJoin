#ifndef RAYJOIN_SCALE_POINTS_H
#define RAYJOIN_SCALE_POINTS_H

#include <vulkan/vulkan.h>

#include <cstdint>
#include <stdexcept>

#include "vk/common/vk_context.h"
#include "vk/common/vk_helpers.h"
#include "vk_mem_alloc.h"

struct alignas(16) SrcPointD {
  double x, y;
};
struct alignas(16) DstPointI64 {
  int64_t x, y;
};
static_assert(sizeof(SrcPointD) == 16);
static_assert(sizeof(DstPointI64) == 16);

struct PushConstantsD2I64 {
  double rx;
  double ry;
  double deltax;
  double deltay;
  uint32_t count;
  uint32_t pad0;
};
static_assert(sizeof(PushConstantsD2I64) == 40);

class ScalePointsPassD2I64 {
 public:
  void init(const VkComputeContext& ctx, const char* spvPath) {
    m_ctx = ctx;

    // Descriptor set layout: binding 0 src storage buffer, binding 1 dst
    // storage buffer
    VkDescriptorSetLayoutBinding b0{};
    b0.binding = 0;
    b0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding b1{};
    b1.binding = 1;
    b1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b1.descriptorCount = 1;
    b1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {b0, b1};

    VkDescriptorSetLayoutCreateInfo dslci{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    VK_CHECK(
        vkCreateDescriptorSetLayout(ctx.device, &dslci, nullptr, &m_setLayout));

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstantsD2I64);

    VkPipelineLayoutCreateInfo plci{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_setLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &plci, nullptr, &m_pipeLayout));

    auto spirv = readSpvU32(spvPath);
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spirv.size() * sizeof(uint32_t);
    smci.pCode = spirv.data();
    VK_CHECK(vkCreateShaderModule(ctx.device, &smci, nullptr, &m_shader));

    VkPipelineShaderStageCreateInfo stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";

    VkComputePipelineCreateInfo cpci{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage = stage;
    cpci.layout = m_pipeLayout;
    VK_CHECK(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &cpci,
                                      nullptr, &m_pipeline));

    // descriptor set
    VkDescriptorSetAllocateInfo dsai{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = ctx.descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &m_setLayout;
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &dsai, &m_descSet));

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
    *this = {};
  }

  void prepareBuffers(uint32_t count) {
    cleanupBuffers();
    m_count = count;

    VkDeviceSize srcSize = VkDeviceSize(sizeof(SrcPointD)) * count;
    VkDeviceSize dstSize = VkDeviceSize(sizeof(DstPointI64)) * count;

    m_srcStaging = vmaCreateBufferSimple(m_ctx.vma, srcSize,
                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VMA_MEMORY_USAGE_CPU_ONLY);

    m_srcDevice = vmaCreateBufferSimple(
        m_ctx.vma, srcSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    m_dstDevice = vmaCreateBufferSimple(
        m_ctx.vma, dstSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkDescriptorBufferInfo srcInfo{m_srcDevice.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo dstInfo{m_dstDevice.buf, 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[2]{};
    wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorCount = 1;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[0].pBufferInfo = &srcInfo;

    wr[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[1].dstSet = m_descSet;
    wr[1].dstBinding = 1;
    wr[1].descriptorCount = 1;
    wr[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[1].pBufferInfo = &dstInfo;

    vkUpdateDescriptorSets(m_ctx.device, 2, wr, 0, nullptr);
  }

  // record + submit internally (simple, reliable)
  void run(const SrcPointD* srcPoints, uint32_t count, double rx, double ry,
           double deltax, double deltay) {
    if (!m_inited)
      throw std::runtime_error("ScalePointsPassD2I64 not initialized");
    if (count != m_count)
      throw std::runtime_error("prepareBuffers(count) mismatch");

    // upload to staging
    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(m_ctx.vma, m_srcStaging.alloc, &mapped));
    std::memcpy(mapped, srcPoints, sizeof(SrcPointD) * count);
    vmaUnmapMemory(m_ctx.vma, m_srcStaging.alloc);

    VkCommandBuffer cmd = beginOneTime(m_ctx.device, m_ctx.cmdPool);

    // copy staging->device
    VkBufferCopy cpy{0, 0, VkDeviceSize(sizeof(SrcPointD)) * count};
    vkCmdCopyBuffer(cmd, m_srcStaging.buf, m_srcDevice.buf, 1, &cpy);

    // barriers (using sync2)
    VkBufferMemoryBarrier2 bSrc{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    bSrc.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    bSrc.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bSrc.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bSrc.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    bSrc.buffer = m_srcDevice.buf;
    bSrc.offset = 0;
    bSrc.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bDst{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    bDst.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    bDst.srcAccessMask = 0;
    bDst.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bDst.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    bDst.buffer = m_dstDevice.buf;
    bDst.offset = 0;
    bDst.size = VK_WHOLE_SIZE;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    VkBufferMemoryBarrier2 barrs[] = {bSrc, bDst};
    dep.bufferMemoryBarrierCount = 2;
    dep.pBufferMemoryBarriers = barrs;
    vkCmdPipelineBarrier2(cmd, &dep);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout,
                            0, 1, &m_descSet, 0, nullptr);

    PushConstantsD2I64 pc{};
    pc.rx = rx;
    pc.ry = ry;
    pc.deltax = deltax;
    pc.deltay = deltay;
    pc.count = count;
    pc.pad0 = 0;

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstantsD2I64), &pc);

    uint32_t groups = (count + 256 - 1) / 256;
    vkCmdDispatch(cmd, groups, 1, 1);

    // ensure writes visible to later passes
    VkBufferMemoryBarrier2 after{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    after.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    after.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    after.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    after.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    after.buffer = m_dstDevice.buf;
    after.offset = 0;
    after.size = VK_WHOLE_SIZE;

    VkDependencyInfo dep2{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep2.bufferMemoryBarrierCount = 1;
    dep2.pBufferMemoryBarriers = &after;
    vkCmdPipelineBarrier2(cmd, &dep2);

    endSubmitWait(m_ctx.device, m_ctx.queue, m_ctx.cmdPool, cmd);
  }

  const AllocBuf& dstBuffer() const { return m_dstDevice; }

 private:
  void cleanupBuffers() {
    vmaDestroyBufferSafe(m_ctx.vma, m_srcStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_srcDevice);
    vmaDestroyBufferSafe(m_ctx.vma, m_dstDevice);
    m_count = 0;
  }

  VkComputeContext m_ctx{};
  bool m_inited = false;
  uint32_t m_count = 0;

  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
  VkShaderModule m_shader = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
  VkDescriptorSet m_descSet = VK_NULL_HANDLE;

  AllocBuf m_srcStaging{};
  AllocBuf m_srcDevice{};
  AllocBuf m_dstDevice{};
};

#endif  // RAYJOIN_SCALE_POINTS_H
