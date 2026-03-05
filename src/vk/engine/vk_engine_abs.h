#ifndef RAYJOIN_VK_ENGINE_ABS_H
#define RAYJOIN_VK_ENGINE_ABS_H
#include "vk_compute_context.h"

namespace rayjoin {
namespace vk {
class VkComputeEngine {
 public:
  VkComputeEngine(const VkComputeContext& ctx, const char* spvPath)
      : m_ctx(ctx) {}

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

  void run() {
    VkCommandBuffer cmd = beginOneTime(m_ctx.device, m_ctx.cmdPool);

    recordCopy(cmd);

    recordBarrier(cmd);

    preDispatch(cmd);

    recordDispatch(cmd);

    recordPostBarrier(cmd);

    endSubmitWait(m_ctx.device, m_ctx.queue, m_ctx.cmdPool, cmd);
  }

 protected:
  void setup(const char* spvPath) {
    createPipeline(spvPath);
    allocateDescriptors();
    createBuffers();
    uploadCPUData();
    recordDescriptors();
  }

  /* ---------- initialization hooks ---------- */

  virtual void createPipeline(const char* spvPath) = 0;
  virtual void allocateDescriptors() = 0;
  virtual void createBuffers() = 0;
  virtual void uploadCPUData() = 0;
  virtual void recordDescriptors() = 0;

  /* ---------- command hooks ---------- */

  virtual void recordCopy(VkCommandBuffer cmd) = 0;
  virtual void recordBarrier(VkCommandBuffer cmd) = 0;
  virtual void preDispatch(VkCommandBuffer cmd) = 0;
  virtual void recordDispatch(VkCommandBuffer cmd) = 0;
  virtual void recordPostBarrier(VkCommandBuffer cmd) = 0;

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
