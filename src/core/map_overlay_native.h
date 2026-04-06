#ifndef RAYJOIN_MAP_OVERLAY_NATIVE_H
#define RAYJOIN_MAP_OVERLAY_NATIVE_H

#include "lsi_native.h"
#include "pip_native.h"
#include "shader/config.h"

namespace rayjoin {

template<typename CONTEXT_T>
class MapOverlayNative {
 public:
  using coord_t = typename CONTEXT_T::coord_t;

  MapOverlayNative() = delete;
  explicit MapOverlayNative(CONTEXT_T& ctx) : ctx_(ctx) {}

  virtual ~MapOverlayNative() = default;

  virtual void Init() = 0;
  virtual void BuildIndex() = 0;
  virtual void IntersectEdge(int query_map_id) = 0;
  virtual void LocateVerticesInOtherMap(int query_map_id) = 0;
  virtual void ComputeOutputPolygons() = 0;
  virtual void WriteResult(const char* path) = 0;

  thrust::host_vector<index_t> get_closet_eids(int im) const {
    thrust::host_vector<index_t> res = closest_eids_[im];
    return res;
  }

  thrust::host_vector<index_t> get_point_in_polygon(int im) const {
    thrust::host_vector<index_t> res = point_in_polygon_[im];
    return res;
  }

  thrust::host_vector<typename LSINative<CONTEXT_T>::xsect_t> get_xsect_edges() const {
    thrust::host_vector<typename LSINative<CONTEXT_T>::xsect_t> res;
    lsi_->CopyTo(res);
    return res;
  }

 protected:
  CONTEXT_T& ctx_;
  thrust::device_vector<index_t> closest_eids_[2];
  thrust::device_vector<index_t> point_in_polygon_[2];
  std::shared_ptr<LSINative<CONTEXT_T>> lsi_;
  std::shared_ptr<PIPNative<CONTEXT_T>> pip_;
};


}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_NATIVE_H
