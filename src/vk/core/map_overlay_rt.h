#ifndef RAYJOIN_MAP_OVERLAY_RT_H
#define RAYJOIN_MAP_OVERLAY_RT_H

#include "vk/core/lsi_rt.h"
#include "vk/rt/rt_engine.h"

namespace rayjoin {
namespace vk {

template <typename CONTEXT_T>
class MapOverlayRT : public MapOverlay<CONTEXT_T> {
 public:
  explicit MapOverlayRT(CONTEXT_T& ctx) : MapOverlay<CONTEXT_T>(ctx) {
    rt_engine_ = std::make_shared<RTEngine>();
    this->lsi_ = std::make_shared<LSIRT<CONTEXT_T>>(ctx, rt_engine_);
    // this->pip_ = std::make_shared<PIPRT<CONTEXT_T>>(ctx, rt_engine_);
  }

  ~MapOverlayRT() override {
    auto& vk_ctx = GetVkComputeContext();

    for (int i = 0; i < 2; i++) {
      vmaDestroyBufferSafe(vk_ctx.vma, closest_eids_buf_[i]);
      vmaDestroyBufferSafe(vk_ctx.vma, point_in_polygon_buf_[i]);
      vmaDestroyBufferSafe(vk_ctx.vma, eid_range_buf_[i]);
    }

    vmaDestroyBufferSafe(vk_ctx.vma, aabbs_buf_);
  }

  void set_config(const QueryConfigRT& config) { config_ = config; }

  void Init() override {
    auto& ctx = this->ctx_;
    auto& lsi = this->lsi_;
    auto& vk_ctx = GetVkComputeContext();

    // -------------------------------
    // TODO:: Initialize RT Engine
    // -------------------------------

    // -------------------------------
    // Scan maps
    // -------------------------------
    size_t max_n_points = 0;
    size_t max_n_edges = 0;

    for (int im = 0; im < 2; im++) {
      auto map = ctx.get_map(im);

      size_t np = map->get_points_num();
      size_t ne = map->get_edges_num();

      map_point_count_[im] = np;
      map_edge_count_[im] = ne;

      // closest edge id per vertex
      closest_eids_buf_[im] = createStorageBuffer<index_t>(vk_ctx.vma, np);

      // point -> polygon id
      point_in_polygon_buf_[im] = createStorageBuffer<index_t>(vk_ctx.vma, np);

      // primitive -> edge range mapping
      eid_range_buf_[im] =
          createStorageBuffer<std::pair<uint32_t, uint32_t>>(vk_ctx.vma, ne);

      max_n_points = std::max(max_n_points, np);
      max_n_edges = std::max(max_n_edges, ne);
    }
    // -------------------------------
    // Allocate AABB primitive buffer
    // -------------------------------
    aabbs_buf_ =
        createStorageBuffer<VkAabbPositionsKHR>(vk_ctx.vma, max_n_edges);
    // -------------------------------
    // Compute total edges
    // -------------------------------
    size_t n_edges = map_edge_count_[0] + map_edge_count_[1];

    // Initialize LSI
    lsi->Init(ctx.get_edge_num() * config_.xsect_factor);

    // TODO: enable pip (disabled for now)
    // this->pip_->Init(max_n_points);
  }

  void BuildIndex() override {}

  void IntersectEdge(int query_map_id) override {}

  void LocateVerticesInOtherMap(int query_map_id) override {}

  void ComputeOutputPolygons() override {}

  void WriteResult(const char* path) override {}

 private:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;

  // -------------------------------
  // GPU buffers
  // -------------------------------
  // per-map: vertex -> closest edge
  AllocBuf closest_eids_buf_[2];
  // per-map: vertex -> polygon id
  AllocBuf point_in_polygon_buf_[2];
  // per-map: primitive -> edge index range
  AllocBuf eid_range_buf_[2];
  // AABB primitives used for RT acceleration structure
  AllocBuf aabbs_buf_;
  // -------------------------------
  // cached map sizes
  // -------------------------------
  size_t map_point_count_[2] = {0, 0};
  size_t map_edge_count_[2] = {0, 0};

  // Queue<xsect_t> xsect_queue_;
};

}  // namespace vk
}  // namespace rayjoin
#endif  // RAYJOIN_MAP_OVERLAY_RT_H
