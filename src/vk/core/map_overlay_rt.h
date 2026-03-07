#ifndef RAYJOIN_MAP_OVERLAY_RT_H
#define RAYJOIN_MAP_OVERLAY_RT_H

#include "vk/core/lsi_rt.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/map.h"
#include "vk/map/vk_debug_readback.h"
#include "vk/rt/primitives.h"
#include "vk/rt/rt_engine.h"

namespace rayjoin {
namespace vk {

template <typename CONTEXT_T>
class MapOverlayRT : public MapOverlay<CONTEXT_T> {
  using map_t = typename CONTEXT_T::map_t;

 public:
  explicit MapOverlayRT(CONTEXT_T& ctx) : MapOverlay<CONTEXT_T>(ctx) {
    rt_engine_ = std::make_shared<RTEngine>();
    this->lsi_ = std::make_shared<LSIRT<CONTEXT_T>>(ctx, rt_engine_);
    // this->pip_ = std::make_shared<PIPRT<CONTEXT_T>>(ctx, rt_engine_);
  }

  void set_config(const QueryConfigRT& config) { config_ = config; }

  void Init() override {
    auto& ctx = this->ctx_;
    auto& lsi = this->lsi_;
    auto& vk_ctx = GetVkComputeContext();

    // -------------------------------
    // TODO:: Initialize RT Engine
    // -------------------------------
    rt_engine_ = std::make_shared<rayjoin::vk::RTEngine>();
    rt_engine_->Init();

    // -------------------------------
    // Scan maps
    // -------------------------------
    size_t max_n_points = 0;
    size_t max_n_edges = 0;

    for (int im = 0; im < 2; im++) {
      std::shared_ptr<map_t> map = ctx.get_map(im);

      size_t np = map->get_points_num();
      size_t ne = map->get_edges_num();

      map_point_count_[im] = np;
      map_edge_count_[im] = ne;
      LOG(INFO) << "Map-" << im << ": points=" << np << ", edges=" << ne;

      // closest edge id per vertex
      closest_eids_buf_[im].Init(sizeof(index_t) * np);
      // point -> polygon id
      point_in_polygon_buf_[im].Init(sizeof(index_t) * np);
      // primitive -> edge range mapping
      eid_range_buf_[im].Init(sizeof(std::pair<uint32_t, uint32_t>) * ne);

      max_n_points = std::max(max_n_points, np);
      max_n_edges = std::max(max_n_edges, ne);
    }
    // -------------------------------
    // Allocate AABB primitive buffer
    // -------------------------------
    // aabbs_buf_ =
    //     createStorageBuffer<VkAabbPositionsKHR>(vk_ctx.vma, max_n_edges);

    // aabbs_buf_. = createStorageBuffer(vk_ctx.vma,
    //                                  sizeof(VkAabbPositionsKHR) *
    //                                  max_n_edges);
    aabbs_buf_.Init(sizeof(VkAabbPositionsKHR) * max_n_edges);
    // -------------------------------
    // Compute total edges
    // -------------------------------
    size_t n_edges = map_edge_count_[0] + map_edge_count_[1];

    // Initialize LSI
    lsi->Init(ctx.get_edge_num() * config_.xsect_factor);

    // TODO: enable pip (disabled for now)
    // this->pip_->Init(max_n_points);
  }

  void BuildIndex() override {
    auto& ctx = this->ctx_;
    auto& vk_ctx = GetVkComputeContext();

    auto ag_iter = config_.ag_iter;
    auto area_enlarge = config_.enlarge;

    for (int im = 0; im < 2; im++) {
      auto map = ctx.get_map(im);

      std::string spvPath = std::string(SHADER_DIR) + "/fill_primitives.spv";

      fill_primitives_pass_ = std::make_unique<FillPrimitives>(
          spvPath.c_str(), map->getPointsBuffer(), map->getEdgesBuffer(),
          map->getScalingBuffer(),  // ← NEW
          aabbs_buf_, eid_range_buf_[im], map_edge_count_[im], ag_iter,
          area_enlarge);

      fill_primitives_pass_->run();

      DebugPrintAABBs(map, aabbs_buf_, eid_range_buf_[im], map_edge_count_[im]);

      traverse_handles_[im] =
          rt_engine_->BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);

      // if (config_.fau) {
      //   clearBuffer(aabbs_buf_);
      // }
    }
  }

  void IntersectEdge(int query_map_id) override {
    int base_map_id = 1 - query_map_id;
    auto lsi = std::dynamic_pointer_cast<LSIRT<CONTEXT_T>>(this->lsi_);

    config_.eid_range = eid_range_[base_map_id];
    config_.handle = traverse_handles_[base_map_id];

    lsi->set_config(config_);
    lsi->Query(query_map_id);
  }

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
  VkDeviceBuf closest_eids_buf_[2];
  // per-map: vertex -> polygon id
  VkDeviceBuf point_in_polygon_buf_[2];
  // per-map: primitive -> edge index range
  VkDeviceBuf eid_range_buf_[2];
  // AABB primitives used for RT acceleration structure
  VkDeviceBuf aabbs_buf_;
  // -------------------------------
  // cached map sizes
  // -------------------------------
  size_t map_point_count_[2] = {0, 0};
  size_t map_edge_count_[2] = {0, 0};

  // -------------------------------
  // Pipelines
  // -------------------------------
  std::unique_ptr<FillPrimitives> fill_primitives_pass_;

  // -------------------------------
  // BVH Handlers
  // -------------------------------
  VkAccelerationStructureKHR traverse_handles_[2];

  std::shared_ptr<VkDeviceBuf> eid_range_[2];

  // Queue<xsect_t> xsect_queue_;

  void DebugPrintAABBs(std::shared_ptr<map_t> map, const VkDeviceBuf& aabbBuf,
                       const VkDeviceBuf& eidRangeBuf,
                       uint32_t edge_count) const {
    uint32_t checkCount = std::min<uint32_t>(edge_count, 10);

    auto gpuAABBs =
        readBackStorageBuffer<VkAabbPositionsKHR>(aabbBuf, checkCount);

    auto gpuRanges = readBackStorageBuffer<std::pair<uint32_t, uint32_t>>(
        eidRangeBuf, checkCount);

    auto gpuEdges =
        readBackStorageBuffer<Edge>(map->getEdgesBuffer(), edge_count);

    auto gpuPts = readBackStorageBuffer<DstPointI64>(map->getPointsBuffer(),
                                                     map->get_points_num());

    auto scaling = readBackStorageBuffer<Scaling<double, int64_t>>(
        map->getScalingBuffer(), 1)[0];

    LOG(INFO) << "Debug AABB validation (first " << checkCount
              << " primitives):";

    for (uint32_t i = 0; i < checkCount; i++) {
      const auto& aabb = gpuAABBs[i];
      const auto& range = gpuRanges[i];

      uint32_t eid = range.first;

      const auto& e = gpuEdges[eid];

      auto& p1 = gpuPts[e.p1_idx];
      auto& p2 = gpuPts[e.p2_idx];

      double x1 = scaling.UnscaleX(p1.x);
      double y1 = scaling.UnscaleY(p1.y);

      double x2 = scaling.UnscaleX(p2.x);
      double y2 = scaling.UnscaleY(p2.y);

      double minx = std::min(x1, x2);
      double maxx = std::max(x1, x2);
      double miny = std::min(y1, y2);
      double maxy = std::max(y1, y2);

      bool inside = (minx >= aabb.minX && maxx <= aabb.maxX &&
                     miny >= aabb.minY && maxy <= aabb.maxY);

      LOG(INFO) << "eid=" << eid << " edge=(" << x1 << "," << y1 << ") -> ("
                << x2 << "," << y2 << ")"
                << " AABB=[(" << aabb.minX << "," << aabb.minY << ") ("
                << aabb.maxX << "," << aabb.maxY << ")]"
                << " contains=" << (inside ? "YES" : "NO");
    }
  }
};

}  // namespace vk
}  // namespace rayjoin
#endif  // RAYJOIN_MAP_OVERLAY_RT_H
