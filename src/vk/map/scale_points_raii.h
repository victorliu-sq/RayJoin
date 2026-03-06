#ifndef RAYJOIN_SCALE_POINTS_D2_I64_RAII_H
#define RAYJOIN_SCALE_POINTS_D2_I64_RAII_H

#include <cstring>
#include <vector>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/engine/vk_engine_abs.h"
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
static_assert(sizeof(Vec2<double>) == sizeof(SrcPointD));
static_assert(alignof(Vec2<double>) == alignof(SrcPointD));

struct PushConstantsD2I64 {
  double rx;
  double ry;
  double deltax;
  double deltay;
  uint32_t count;
  uint32_t pad0;
};

class ScalePointsPassD2I64RAII : public VkComputeEngine {
 public:
  ScalePointsPassD2I64RAII(const char* spvPath, const VkDeviceBuf& srcBuffer,
                           const VkDeviceBuf& dstBuffer,
                           const VkDeviceBuf& scalingBuffer, uint32_t count)
      : VkComputeEngine(),
        m_srcDevice(srcBuffer),
        m_dstDevice(dstBuffer),
        m_scalingDevice(scalingBuffer),
        m_count(count) {
    createPipeline(spvPath);
    allocateDescriptors();
    recordDescriptors();
  }

 protected:
  /* ---------- Pipeline ---------- */
  void createPipeline(const char* spvPath) override {
    VkDescriptorSetLayoutBinding bindings[3]{};

    for (uint32_t i = 0; i < 3; ++i) {
      bindings[i].binding = i;
      bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 3;
    dsl.pBindings = bindings;

    VK_CHECK(
        vkCreateDescriptorSetLayout(m_ctx.device, &dsl, nullptr, &m_setLayout));

    VkPipelineLayoutCreateInfo pl{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;

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

  /* ---------- Descriptors ---------- */
  void allocateDescriptors() override {
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}};

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
    VkDescriptorBufferInfo srcInfo{m_srcDevice.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo dstInfo{m_dstDevice.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo scalingInfo{m_scalingDevice.Buf(), 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[3]{};

    wr[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr[0].dstSet = m_descSet;
    wr[0].dstBinding = 0;
    wr[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr[0].descriptorCount = 1;
    wr[0].pBufferInfo = &srcInfo;

    wr[1] = wr[0];
    wr[1].dstBinding = 1;
    wr[1].pBufferInfo = &dstInfo;

    wr[2] = wr[0];
    wr[2].dstBinding = 2;
    wr[2].pBufferInfo = &scalingInfo;

    vkUpdateDescriptorSets(m_ctx.device, 3, wr, 0, nullptr);
  }

  /* ---------- Dispatch ---------- */
  void recordDispatch(VkCommandBuffer cmd) override {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout,
                            0, 1, &m_descSet, 0, nullptr);

    uint32_t groups = (m_count + 255) / 256;

    vkCmdDispatch(cmd, groups, 1, 1);
  }

 private:
  uint32_t m_count{};

  const VkDeviceBuf& m_srcDevice;
  const VkDeviceBuf& m_dstDevice;
  const VkDeviceBuf& m_scalingDevice;
};

}  // namespace vk
}  // namespace rayjoin

#endif