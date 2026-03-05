#ifndef RAYJOIN_VK_ENGINE_H
#define RAYJOIN_VK_ENGINE_H
#include "vk_buffer_alloc.h"
#include "vk_context.h"

namespace rayjoin {
namespace vk {
class VkEngine {
 public:
 private:
  VkContext ctx_;
  VkBufferAlloc buffer_alloc_;
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_VK_ENGINE_H
