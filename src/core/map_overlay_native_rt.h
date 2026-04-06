#ifndef RAYJOIN_MAP_OVERLAY_NATIVE_RT_H
#define RAYJOIN_MAP_OVERLAY_NATIVE_RT_H

#include <memory>

#include "map_overlay_native.h"
#include "query_config.h"
#include "rt/rt_engine.h"

namespace rayjoin {

template<typename CONTEXT_T>
class MapOverlayNativeRT : public MapOverlayNative<CONTEXT_T> {
 public:
  using base_t = MapOverlayNative<CONTEXT_T>;
  using coord_t = typename CONTEXT_T::coord_t;
  using map_t = typename CONTEXT_T::map_t;
  using point_t = typename map_t::point_t;
  using edge_t = typename map_t::edge_t;

  explicit MapOverlayNativeRT(CONTEXT_T& ctx) : MapOverlayNative<CONTEXT_T>(ctx) { rt_engine_ = std::make_shared<RTEngine>(); }

  void set_config(const QueryConfigRT& config) { config_ = config; }

  void Init() override {
    auto& ctx = this->ctx_;
    auto exec_root = ctx.get_exec_root();

    RTConfig rt_config = get_default_rt_config(exec_root);
    rt_engine_->Init(rt_config);

    size_t max_n_points = 0;
    size_t max_n_edges = 0;

    FOR2 {
      auto map = ctx.get_map(im);
      CHECK(map != nullptr) << "MapOverlayNativeRT::Init: map " << im << " is null";

      const size_t np = map->get_points_num();
      const size_t ne = map->get_edges_num();

      this->closest_eids_[im].resize(np, DONTKNOW);
      this->point_in_polygon_[im].resize(np, DONTKNOW);

      eid_range_[im] = std::make_shared<thrust::device_vector<thrust::pair<size_t, size_t>>>();
      eid_range_[im]->clear();
      eid_range_[im]->reserve(ne);

      max_n_points = std::max(max_n_points, np);
      max_n_edges = std::max(max_n_edges, ne);
    }

    aabbs_.clear();
    aabbs_.reserve(max_n_edges);

    FOR2 { traverse_handles_[im] = 0; }

    LOG(INFO) << "MapOverlayNativeRT::Init finished. "
              << "max_n_points=" << max_n_points << ", max_n_edges=" << max_n_edges;
  }

  void BuildIndex() override { LOG(FATAL) << "MapOverlayNativeRT::BuildIndex is not implemented yet"; }

  void IntersectEdge(int query_map_id) override { LOG(FATAL) << "MapOverlayNativeRT::IntersectEdge is not implemented yet"; }

  void LocateVerticesInOtherMap(int query_map_id) override { LOG(FATAL) << "MapOverlayNativeRT::LocateVerticesInOtherMap is not implemented yet"; }

  void ComputeOutputPolygons() override { LOG(FATAL) << "MapOverlayNativeRT::ComputeOutputPolygons is not implemented yet"; }

  void WriteResult(const char* path) override { LOG(FATAL) << "MapOverlayNativeRT::WriteResult is not implemented yet"; }

 private:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;

  OptixTraversableHandle traverse_handles_[2]{};
  thrust::device_vector<OptixAabb> aabbs_;
  std::shared_ptr<thrust::device_vector<thrust::pair<size_t, size_t>>> eid_range_[2];
};

}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_NATIVE_RT_H
