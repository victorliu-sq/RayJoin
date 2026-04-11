#ifndef RAYJOIN_VK_GLOBAL_CONTEXT_H
#define RAYJOIN_VK_GLOBAL_CONTEXT_H

#include <vk_mem_alloc.h>

#include "vk/engine/vk_compute_context.h"

namespace rayjoin::vk {
class VkGlobalRuntime {
 public:
  static VkComputeContext ctx;  // Compute Context

  VkGlobalRuntime() { initVkComputeContext(ctx); }

  ~VkGlobalRuntime() {
    VkInstance instance = ctx.instance;
    destroyVkComputeContext(ctx);
    if (instance) {
      vkDestroyInstance(instance, nullptr);
    }
  }
};

static inline VkComputeContext& GetVkComputeContext() { return VkGlobalRuntime::ctx; }

// static inline VkGlobalRuntime CreateVkGlobalRuntime() { return VkGlobalRuntime(); }
static inline std::unique_ptr<VkGlobalRuntime> CreateVkGlobalRuntime() { return std::make_unique<VkGlobalRuntime>(); }
}  // namespace rayjoin::vk

#endif  // RAYJOIN_VK_GLOBAL_CONTEXT_H
