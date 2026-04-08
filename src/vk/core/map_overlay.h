#ifndef RAYJOIN_MAP_OVERLAY_H
#define RAYJOIN_MAP_OVERLAY_H

#include "vk/_NOUSE_rt/_NOUSE_rt_engine.h"
#include "vk/core/_NOUSE_lsi.h"

namespace rayjoin {
namespace vk {
template<typename CONTEXT_T>
class MapOverlay {
 public:
  using coord_t = typename CONTEXT_T::coord_t;
  using internal_coord_t = typename CONTEXT_T::internal_coord_t;
  using coefficient_t = typename CONTEXT_T::coefficient_t;
  // using xsect_t = dev::Intersection<internal_coord_t>;

  MapOverlay() = delete;
  explicit MapOverlay(CONTEXT_T& ctx) : ctx_(ctx) {}

  virtual ~MapOverlay() = default;

  virtual void Init() = 0;

  virtual void BuildIndex() = 0;

  virtual void IntersectEdge(int query_map_id) = 0;

  virtual void LocateVerticesInOtherMap(int query_map_id) = 0;

  virtual void ComputeOutputPolygons() = 0;

  virtual void WriteResult(const char* path) = 0;

 protected:
  CONTEXT_T& ctx_;  // reference to inputs initialized from outside

  // std::shared_ptr<LSI<CONTEXT_T>> lsi_;  // algo1
  // std::shared_ptr<PIP<CONTEXT_T>> pip_; // algo2

  // thrust::device_vector<polygon_id_t> closest_eids_[2]; for pip
  // thrust::device_vector<polygon_id_t> point_in_polygon_[2]; for results
};
}  // namespace vk
}  // namespace rayjoin
   // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_H
