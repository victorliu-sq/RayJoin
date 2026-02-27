#ifndef RAYJOIN_QUERY_CONFIG_H
#define RAYJOIN_QUERY_CONFIG_H

namespace rayjoin {
namespace vk {

struct QueryConfigRT {
  bool profile = false;
  bool fau = true;
  float xsect_factor = 1.5;
  int win;
  int ag_iter;
  float enlarge;
  int ag;
  // OptixTraversableHandle handle;
  // std::shared_ptr<thrust::device_vector<thrust::pair<size_t, size_t>>>
  //     eid_range;
};

}  // namespace vk

}  // namespace rayjoin

#endif  // RAYJOIN_QUERY_CONFIG_H
