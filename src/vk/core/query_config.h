#ifndef RAYJOIN_QUERY_CONFIG_H
#define RAYJOIN_QUERY_CONFIG_H
#include "vk/engine/vk_buffer.h"

namespace rayjoin {
namespace vk {

struct EidRange {
  uint32_t first = 0;
  uint32_t second = 0;
};

struct QueryConfigRT {
  // algorithm knobs
  bool profile = false;
  bool fau = true;
  float xsect_factor = 1.5f;
  int win = 0;
  int ag_iter = 0;
  float enlarge = 0.0f;
  int ag = 0;

  // Optix RT runtime state
  // OptixTraversableHandle handle;
  // std::shared_ptr<thrust::device_vector<thrust::pair<size_t, size_t>>>
  //     eid_range;

  // runtime bindings for current query
  VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
  std::shared_ptr<VkDeviceBuf> eid_range = nullptr;
};

}  // namespace vk

}  // namespace rayjoin

#endif  // RAYJOIN_QUERY_CONFIG_H
