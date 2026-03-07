#ifndef RAYJOIN_VK_ENGINE_ABS_H
#define RAYJOIN_VK_ENGINE_ABS_H
#include "vk_compute_context.h"

namespace rayjoin {
namespace vk {
class VkComputeEngine {
 public:
  VkComputeEngine()
      : m_ctx(GetVkComputeContext()) {}

  virtual ~VkComputeEngine() {
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

  virtual void run() {
    VkCommandBuffer cmd = beginOneTime(m_ctx.device, m_ctx.cmdPool);
    recordDispatch(cmd);
    endSubmitWait(m_ctx.device, m_ctx.queue, m_ctx.cmdPool, cmd);
  }

 protected:
  /* ---------- initialization hooks ---------- */
  virtual void createPipeline(const char* spvPath) = 0;
  virtual void allocateDescriptors() = 0;
  virtual void recordDescriptors() = 0;

  /* ---------- command hooks ---------- */
  virtual void recordDispatch(VkCommandBuffer cmd) = 0;

 protected:
  VkComputeContext m_ctx{};

  VkPipelineLayout m_pipeLayout = VK_NULL_HANDLE;
  VkShaderModule m_shader = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_descPool = VK_NULL_HANDLE;
  VkDescriptorSet m_descSet = VK_NULL_HANDLE;
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_VK_ENGINE_ABS_H
