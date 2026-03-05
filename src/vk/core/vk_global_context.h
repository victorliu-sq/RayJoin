#ifndef RAYJOIN_VK_GLOBAL_CONTEXT_H
#define RAYJOIN_VK_GLOBAL_CONTEXT_H

#include "vk/engine/vk_compute_context.h"

class VkGlobalRuntime {
 public:
  static VkComputeContext ctx; // Compute Context

  VkGlobalRuntime() {
    initVkComputeContext(ctx);
  }

  ~VkGlobalRuntime() {
    VkInstance instance = ctx.instance;
    destroyVkComputeContext(ctx);
    if (instance) {
      vkDestroyInstance(instance, nullptr);
    }
  }
};

static inline VkComputeContext& GetVkComputeContext() {
  return VkGlobalRuntime::ctx;
}

static inline std::unique_ptr<VkGlobalRuntime> CreateVkGlobalRuntime() {
  return std::make_unique<VkGlobalRuntime>();
}

#endif  // RAYJOIN_VK_GLOBAL_CONTEXT_H
