#ifndef RAYJOIN_SCALE_POINTS_D2_I64_RAII_H
#define RAYJOIN_SCALE_POINTS_D2_I64_RAII_H

#include <cstring>
#include <vector>

#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_helpers.h"
#include "vk_mem_alloc.h"

namespace rayjoin {
namespace vk {

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

class ScalePointsPassD2I64RAII {
 public:
  ScalePointsPassD2I64RAII(const char* spvPath,
                           const std::vector<SrcPointD>& srcPoints, double rx,
                           double ry, double deltax, double deltay)
      : m_ctx(GetVkComputeContext()),
        m_rx(rx),
        m_ry(ry),
        m_deltax(deltax),
        m_deltay(deltay) {
    m_count = (uint32_t) srcPoints.size();

    createPipeline(spvPath);
    allocateDescriptors();
    createBuffers();
    uploadCPUData(srcPoints);
    updateDescriptors();
  }

  ~ScalePointsPassD2I64RAII() {
    cleanupBuffers();

    if (m_pipeline) {
      vkDestroyPipeline(m_ctx.device, m_pipeline, nullptr);
    }

    if (m_shader) {
      vkDestroyShaderModule(m_ctx.device, m_shader, nullptr);
    }

    if (m_pipeLayout) {
      vkDestroyPipelineLayout(m_ctx.device, m_pipeLayout, nullptr);
    }

    if (m_setLayout) {
      vkDestroyDescriptorSetLayout(m_ctx.device, m_setLayout, nullptr);
    }

    if (m_descPool) {
      vkDestroyDescriptorPool(m_ctx.device, m_descPool, nullptr);
    }
  }

  AllocBuf dstBuffer() { return m_dstDevice; }

  void run() {
    VkCommandBuffer cmd = beginOneTime(m_ctx.device, m_ctx.cmdPool);

    recordCopy(cmd);
    recordBarrier(cmd);
    recordDispatch(cmd);
    recordPostBarrier(cmd);

    endSubmitWait(m_ctx.device, m_ctx.queue, m_ctx.cmdPool, cmd);
  }

  const AllocBuf& dstBuffer() const { return m_dstDevice; }

 private:
  struct Push {
    double rx;
    double ry;
    double deltax;
    double deltay;
    uint32_t count;
    uint32_t pad0;
  };

  void createPipeline(const char* spvPath) {
    VkDescriptorSetLayoutBinding bindings[2]{};

    for (int i = 0; i < 2; ++i) {
      bindings[i].binding = i;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 2;
    dsl.pBindings = bindings;

    VK_CHECK(
        vkCreateDescriptorSetLayout(m_ctx.device, &dsl, nullptr, &m_setLayout));

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.size = sizeof(Push);

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

  void allocateDescriptors() {
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}};

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

  void createBuffers() {
    VkDeviceSize srcSize = sizeof(SrcPointD) * m_count;
    VkDeviceSize dstSize = sizeof(DstPointI64) * m_count;

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
  }

  void uploadCPUData(const std::vector<SrcPointD>& src) {
    void* mapped;

    VK_CHECK(vmaMapMemory(m_ctx.vma, m_srcStaging.alloc, &mapped));
    memcpy(mapped, src.data(), sizeof(SrcPointD) * m_count);
    vmaUnmapMemory(m_ctx.vma, m_srcStaging.alloc);
  }

  void updateDescriptors() {
    VkDescriptorBufferInfo srcInfo{m_srcDevice.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo dstInfo{m_dstDevice.buf, 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[2]{};

    wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[0].descriptorCount = 1;
    wr[0].pBufferInfo = &srcInfo;

    wr[1] = wr[0];
    wr[1].dstBinding = 1;
    wr[1].pBufferInfo = &dstInfo;

    vkUpdateDescriptorSets(m_ctx.device, 2, wr, 0, nullptr);
  }

  void recordCopy(VkCommandBuffer cmd) {
    VkBufferCopy copy{0, 0, sizeof(SrcPointD) * m_count};

    vkCmdCopyBuffer(cmd, m_srcStaging.buf, m_srcDevice.buf, 1, &copy);
  }

  void recordBarrier(VkCommandBuffer cmd) {
    VkBufferMemoryBarrier2 bars[2]{};

    bars[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bars[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    bars[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bars[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bars[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    bars[0].buffer = m_srcDevice.buf;
    bars[0].size = VK_WHOLE_SIZE;

    bars[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bars[1].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bars[1].dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    bars[1].buffer = m_dstDevice.buf;
    bars[1].size = VK_WHOLE_SIZE;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.bufferMemoryBarrierCount = 2;
    dep.pBufferMemoryBarriers = bars;

    vkCmdPipelineBarrier2(cmd, &dep);
  }

  void recordDispatch(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout,
                            0, 1, &m_descSet, 0, nullptr);

    Push pc{m_rx, m_ry, m_deltax, m_deltay, m_count, 0};

    vkCmdPushConstants(cmd, m_pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(pc), &pc);

    uint32_t groups = (m_count + 255) / 256;

    vkCmdDispatch(cmd, groups, 1, 1);
  }

  void recordPostBarrier(VkCommandBuffer cmd) {
    VkBufferMemoryBarrier2 after{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};

    after.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    after.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    after.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    after.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    after.buffer = m_dstDevice.buf;
    after.size = VK_WHOLE_SIZE;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.bufferMemoryBarrierCount = 1;
    dep.pBufferMemoryBarriers = &after;

    vkCmdPipelineBarrier2(cmd, &dep);
  }

  void cleanupBuffers() {
    vmaDestroyBufferSafe(m_ctx.vma, m_srcStaging);
    vmaDestroyBufferSafe(m_ctx.vma, m_srcDevice);
    vmaDestroyBufferSafe(m_ctx.vma, m_dstDevice);
  }

 private:
  const VkComputeContext& m_ctx{};

  VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
  VkShaderModule m_shader = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_descPool = VK_NULL_HANDLE;
  VkDescriptorSet m_descSet = VK_NULL_HANDLE;

  uint32_t m_count = 0;

  double m_rx{};
  double m_ry{};
  double m_deltax{};
  double m_deltay{};

  AllocBuf m_srcStaging{};
  AllocBuf m_srcDevice{};
  AllocBuf m_dstDevice{};
};

}  // namespace vk
}  // namespace rayjoin

#endif