#ifndef RAYJOIN_PIP_FINALIZE_PASS_NS_H
#define RAYJOIN_PIP_FINALIZE_PASS_NS_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "deps/vulkansdk/x86_64/include/vulkan/vulkan_core.h"
#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_engine_abs.h"


namespace rayjoin {
namespace vk {
class PIPFinalizePassNS : public VkComputeEngine {
 public:
  struct LaunchParamsPIPFinalize {
    uint32_t point_count;
    uint32_t exterior_face_id;
    uint32_t _pad0;
    uint32_t _pad1;
  };

  PIPFinalizePassNS(const char* spv_path,
                    uint32_t point_count,
                    uint32_t exterior_face_id,
                    const VkDeviceBuf& base_edges_buf,
                    const VkDeviceBuf& base_points_buf,
                    const VkDeviceBuf& closest_eids_buf,
                    const VkDeviceBuf& point_in_polygon_buf) :
      m_baseEdgesBuf(base_edges_buf), m_basePointsBuf(base_points_buf), m_closestEidsBuf(closest_eids_buf), m_pointInPolygonBuf(point_in_polygon_buf),
      m_params{point_count, exterior_face_id, 0u, 0u} {
    createDescriptorSetLayout();
    createPipelineLayout();
    createPipeline(spv_path);
    allocateDescriptors();
    uploadParams();
    recordDescriptors();
  }

  ~PIPFinalizePassNS() override = default;

 protected:
  void createPipeline(const char* spvPath) override {
    auto spirv = readSpvU32(spvPath);

    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spirv.size() * sizeof(uint32_t);
    smci.pCode = spirv.data();

    VK_CHECK(vkCreateShaderModule(m_ctx.device, &smci, nullptr, &m_shader));

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = m_shader;
    stage.pName = "main";

    VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage = stage;
    pci.layout = m_pipeLayout;

    VK_CHECK(vkCreateComputePipelines(m_ctx.device, VK_NULL_HANDLE, 1, &pci, nullptr, &m_pipeline));
  }

  void allocateDescriptors() override {
    VkDescriptorPoolSize sizes[1]{};
    sizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5};

    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = sizes;

    VK_CHECK(vkCreateDescriptorPool(m_ctx.device, &pi, nullptr, &m_descPool));

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_descPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_ctx.device, &ai, &m_descSet));
  }

  void recordDescriptors() override {
    VkDescriptorBufferInfo paramsInfo{m_paramsBuf.Buf(), 0, sizeof(LaunchParamsPIPFinalize)};
    VkDescriptorBufferInfo baseEdgesInfo{m_baseEdgesBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo basePointsInfo{m_basePointsBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo closestEidsInfo{m_closestEidsBuf.Buf(), 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo pointInPolygonInfo{m_pointInPolygonBuf.Buf(), 0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet wr[5]{};

    auto set_sb = [&](int i, uint32_t binding, VkDescriptorBufferInfo* info) {
      wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wr[i].dstSet = m_descSet;
      wr[i].dstBinding = binding;
      wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      wr[i].descriptorCount = 1;
      wr[i].pBufferInfo = info;
    };

    set_sb(0, 0, &paramsInfo);
    set_sb(1, 1, &baseEdgesInfo);
    set_sb(2, 2, &basePointsInfo);
    set_sb(3, 3, &closestEidsInfo);
    set_sb(4, 4, &pointInPolygonInfo);

    vkUpdateDescriptorSets(m_ctx.device, 5, wr, 0, nullptr);
  }

  void recordDispatch(VkCommandBuffer cmd) override {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeLayout, 0, 1, &m_descSet, 0, nullptr);

    const uint32_t groupCountX = (m_params.point_count + 255u) / 256u;
    vkCmdDispatch(cmd, groupCountX, 1, 1);
  }

 private:
  void createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding bindings[5]{};

    bindings[0] = MakeBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    bindings[1] = MakeBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    bindings[2] = MakeBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    bindings[3] = MakeBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    bindings[4] = MakeBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 5;
    ci.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx.device, &ci, nullptr, &m_setLayout));
  }

  void createPipelineLayout() {
    VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &m_setLayout;

    VK_CHECK(vkCreatePipelineLayout(m_ctx.device, &ci, nullptr, &m_pipeLayout));
  }

  void uploadParams() {
    m_paramsBuf.Init(sizeof(LaunchParamsPIPFinalize));

    VkStagingBuf staging(sizeof(LaunchParamsPIPFinalize));
    std::vector<uint8_t> bytes(sizeof(LaunchParamsPIPFinalize));
    std::memcpy(bytes.data(), &m_params, sizeof(LaunchParamsPIPFinalize));
    staging.Host2Stage(bytes);
    staging.Stage2Device(m_paramsBuf, sizeof(LaunchParamsPIPFinalize));
  }

  static VkDescriptorSetLayoutBinding MakeBinding(uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = count;
    b.stageFlags = stages;
    return b;
  }

 private:
  const VkDeviceBuf& m_baseEdgesBuf;
  const VkDeviceBuf& m_basePointsBuf;
  const VkDeviceBuf& m_closestEidsBuf;
  const VkDeviceBuf& m_pointInPolygonBuf;

  LaunchParamsPIPFinalize m_params{};
  VkDeviceBuf m_paramsBuf{};
};
}  // namespace vk
}  // namespace rayjoin


#endif  // RAYJOIN_PIP_FINALIZE_PASS_NS_H
