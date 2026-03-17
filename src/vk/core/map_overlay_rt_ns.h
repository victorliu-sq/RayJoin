#ifndef RAYJOIN_MAP_OVERLAY_RT_NS_H
#define RAYJOIN_MAP_OVERLAY_RT_NS_H

#include "map_overlay_ns.h"
#include "query_config.h"
#include "vk/core/lsi_rt.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/lsi_finalize_pass_ns.h"
#include "vk/map/lsi_rt_pass.h"
#include "vk/map/map.h"
#include "vk/map/vk_debug_readback.h"
#include "vk/rt/as_scene.h"
#include "vk/rt/primitive_ns.h"
#include "vk/rt/rt_engine.h"

namespace rayjoin {
namespace vk {

template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
struct Intersection {
  double x;
  double y;

  uint64_t eid0;
  uint64_t eid1;

  uint mid_point_polygon_id;
  uint pad;
};

template<typename CONTEXT_NS_T>
  requires ContextNSType<CONTEXT_NS_T>
class MapOverlayRTNS : public MapOverlayNS<CONTEXT_NS_T> {
  using coord_t = CONTEXT_NS_T::coord_t;
  using map_t = CONTEXT_NS_T::map_t;
  using point_t = CONTEXT_NS_T::point_t;
  using edge_t = CONTEXT_NS_T::edge_t;
  using xsect_t = Intersection<coord_t>;

 public:
  explicit MapOverlayRTNS(CONTEXT_NS_T &ctx) : MapOverlayNS<CONTEXT_NS_T>(ctx) {}

  void set_config(const QueryConfigRT &config) { config_ = config; }

  void Init() override {
    auto &ctx = this->ctx_;
    // auto &lsi = this->lsi_;
    // auto &vk_ctx = GetVkComputeContext();
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
      eid_range_buf_[im].Init(sizeof(EidRange) * ne);

      max_n_points = std::max(max_n_points, np);
      max_n_edges = std::max(max_n_edges, ne);
    }

    // -------------------------------
    // Allocate AABB primitive buffer
    // -------------------------------
    aabbs_buf_.Init(sizeof(VkAabbPositionsKHR) * max_n_edges);

    // -------------------------------
    // Compute total edges
    // -------------------------------
    size_t n_edges = map_edge_count_[0] + map_edge_count_[1];

    // Initialize LSI
    // lsi->Init(ctx.get_edge_num() * config_.xsect_factor);

    // -------------------------------
    // Initialize RT Engine
    // -------------------------------
    // rt_engine_->Init();

    // std::string rgen_spv = std::string(SHADER_DIR) + "/rt/lsi_rgen.spv";
    // std::string rint_spv = std::string(SHADER_DIR) + "/rt/lsi_rint.spv";
    // std::string rahit_spv = std::string(SHADER_DIR) + "/rt/lsi_rahit.spv";
    // std::string rchit_spv = std::string(SHADER_DIR) + "/rt/lsi_rchit.spv";
    // std::string rmiss_spv = std::string(SHADER_DIR) + "/rt/lsi_rmiss.spv";
    // rt_engine_->InitLSIPipeline(rgen_spv.c_str(), rint_spv.c_str(), rahit_spv.c_str(), rchit_spv.c_str(), rmiss_spv.c_str());

    // TODO: enable pip (disabled for now)
    // this->pip_->Init(max_n_points);

    // Intersect Edges
    xsect_capacity_ = this->ctx_.get_edge_num() * config_.xsect_factor;
    xsect_buf_.Init(sizeof(xsect_t) * xsect_capacity_);
    xsect_counter_buf_.Init(sizeof(uint32_t));
    prof_counter_buf_.Init(sizeof(uint32_t) * 20);
  }

  void BuildIndex() override {
    auto &ctx = this->ctx_;
    auto &vk_ctx = GetVkComputeContext();

    auto ag_iter = config_.ag_iter;
    auto area_enlarge = config_.enlarge;

    for (int im = 0; im < 2; im++) {
      auto map = ctx.get_map(im);

      std::string spvPath = std::string(SHADER_DIR_NS) + "/fill_primitives_ns.spv";

      fill_primitives_ns_pass_ = std::make_unique<FillPrimitivesNS>(
          spvPath.c_str(), map->getPointsBuffer(), map->getEdgesBuffer(), aabbs_buf_, eid_range_buf_[im], map_edge_count_[im], ag_iter, area_enlarge);
      fill_primitives_ns_pass_->run();
      DebugPrintAABBs(map, aabbs_buf_, eid_range_buf_[im], map_edge_count_[im]);

      LOG(INFO) << "Map-" << im << " builds " << map_edge_count_[im] << " primtives.";
      accel_[im].BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);

      // traverse_handles_[im] = rt_engine_->BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);
      // traverse_handles_[im] = rt_engine_->BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);

      // if (config_.fau) {
      //   clearBuffer(aabbs_buf_);
      // }
    }
  }

  // void IntersectEdge(int query_map_id) override {
  //   const int base_map_id = 1 - query_map_id;
  //
  //   auto query_map = this->ctx_.get_map(query_map_id);
  //   auto base_map = this->ctx_.get_map(base_map_id);
  //
  //   if (!query_map || !base_map) {
  //     throw std::runtime_error("IntersectEdge(): null map");
  //   }
  //
  //   std::string rgen_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rgen_ns.spv";
  //   std::string rint_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rint_ns.spv";
  //   std::string rahit_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rahit_ns.spv";
  //   std::string rchit_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rchit_ns.spv";
  //   std::string rmiss_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rmiss_ns.spv";
  //
  //   LSIIntersectRTPassNS pass(rgen_spv.c_str(),
  //                             rint_spv.c_str(),
  //                             rahit_spv.c_str(),
  //                             rchit_spv.c_str(),
  //                             rmiss_spv.c_str(),
  //                             accel_[base_map_id].GetTraverseHandle(),
  //                             eid_range_buf_[base_map_id],
  //                             base_map->getPointsBuffer(),
  //                             base_map->getEdgesBuffer(),
  //                             query_map->getPointsBuffer(),
  //                             query_map->getEdgesBuffer(),
  //                             xsect_buf_,
  //                             xsect_counter_buf_,
  //                             prof_counter_buf_,
  //                             static_cast<uint32_t>(query_map_id),
  //                             static_cast<uint32_t>(query_map->get_edges_num()),
  //                             static_cast<uint32_t>(xsect_capacity_));
  //
  //   pass.run();
  //
  //   this->DebugPrintLSIProfiling(query_map_id);
  // }

  void IntersectEdge(int query_map_id) override {
    const int base_map_id = 1 - query_map_id;

    auto query_map = this->ctx_.get_map(query_map_id);
    auto base_map = this->ctx_.get_map(base_map_id);

    if (!query_map || !base_map) {
      throw std::runtime_error("IntersectEdge(): null map");
    }

    std::string rgen_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rgen_ns.spv";
    std::string rint_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rint_ns.spv";
    std::string rahit_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rahit_ns.spv";
    std::string rchit_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rchit_ns.spv";
    std::string rmiss_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rmiss_ns.spv";

    LSIIntersectRTPassNS pass(rgen_spv.c_str(),
                              rint_spv.c_str(),
                              rahit_spv.c_str(),
                              rchit_spv.c_str(),
                              rmiss_spv.c_str(),
                              accel_[base_map_id].GetTraverseHandle(),
                              eid_range_buf_[base_map_id],
                              base_map->getPointsBuffer(),
                              base_map->getEdgesBuffer(),
                              query_map->getPointsBuffer(),
                              query_map->getEdgesBuffer(),
                              xsect_buf_,
                              xsect_counter_buf_,
                              prof_counter_buf_,
                              static_cast<uint32_t>(query_map_id),
                              static_cast<uint32_t>(query_map->get_edges_num()),
                              static_cast<uint32_t>(xsect_capacity_));

    pass.run();

    std::string finalize_spv = std::string(SHADER_DIR_NS) + "/lsi_finalize_ns.spv";
    LSIFinalizePassNS finalize_pass(finalize_spv.c_str(),
                                    static_cast<uint32_t>(query_map_id),
                                    static_cast<uint32_t>(query_map->get_edges_num()),
                                    static_cast<uint32_t>(xsect_capacity_),
                                    base_map->getEdgesBuffer(),
                                    base_map->getPointsBuffer(),
                                    query_map->getEdgesBuffer(),
                                    query_map->getPointsBuffer(),
                                    xsect_buf_,
                                    xsect_counter_buf_);
    finalize_pass.run();

    this->DebugPrintLSIProfiling(query_map_id);
    this->DebugPrintIntersectionsDetailed(query_map_id);
  }

  void LocateVerticesInOtherMap(int query_map_id) override {}

  void ComputeOutputPolygons() override {}

  void WriteResult(const char *path) override {}

 private:
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

  // --------------------------------
  // Build Index
  std::unique_ptr<FillPrimitivesNS> fill_primitives_ns_pass_;
  AccelStructScene accel_[2];
  void DebugPrintAABBs(std::shared_ptr<map_t> map, const VkDeviceBuf &aabbBuf, const VkDeviceBuf &eidRangeBuf, uint32_t edge_count) const {
    const uint32_t checkCount = std::min<uint32_t>(edge_count, 10);

    auto gpuAABBs = readBackStorageBuffer<VkAabbPositionsKHR>(aabbBuf, checkCount);
    auto gpuRanges = readBackStorageBuffer<EidRange>(eidRangeBuf, checkCount);
    auto gpuEdges = readBackStorageBuffer<edge_t>(map->getEdgesBuffer(), edge_count);
    auto gpuPts = readBackStorageBuffer<point_t>(map->getPointsBuffer(), map->get_points_num());

    LOG(INFO) << "Debug AABB validation (first " << checkCount << " primitives):";

    for (uint32_t i = 0; i < checkCount; ++i) {
      const auto &aabb = gpuAABBs[i];
      const auto &range = gpuRanges[i];

      const uint32_t eid = range.first;

      if (eid >= gpuEdges.size()) {
        LOG(ERROR) << "Primitive " << i << ": invalid eid in range buffer, eid=" << eid << ", edge_count=" << gpuEdges.size();
        continue;
      }

      const auto &e = gpuEdges[eid];

      if (e.p1_idx >= gpuPts.size() || e.p2_idx >= gpuPts.size()) {
        LOG(ERROR) << "Primitive " << i << ": invalid point indices for eid=" << eid << ", p1_idx=" << e.p1_idx << ", p2_idx=" << e.p2_idx
                   << ", point_count=" << gpuPts.size();
        continue;
      }

      const auto &p1 = gpuPts[e.p1_idx];
      const auto &p2 = gpuPts[e.p2_idx];

      const double x1 = p1.x;
      const double y1 = p1.y;
      const double x2 = p2.x;
      const double y2 = p2.y;

      const double minx = std::min(x1, x2);
      const double maxx = std::max(x1, x2);
      const double miny = std::min(y1, y2);
      const double maxy = std::max(y1, y2);

      const bool inside = (minx >= static_cast<double>(aabb.minX) && maxx <= static_cast<double>(aabb.maxX) &&
                           miny >= static_cast<double>(aabb.minY) && maxy <= static_cast<double>(aabb.maxY));

      LOG(INFO) << "primitive[" << i << "]"
                << " eid_range=[" << range.first << "," << range.second << ")"
                << " eid=" << eid;

      LOG(INFO) << "  edge=(" << x1 << "," << y1 << ") -> (" << x2 << "," << y2 << ")";

      LOG(INFO) << "  edge_bbox=[(" << minx << "," << miny << ") (" << maxx << "," << maxy << ")]";

      LOG(INFO) << "  gpu_aabb=[(" << aabb.minX << "," << aabb.minY << "," << aabb.minZ << ") (" << aabb.maxX << "," << aabb.maxY << "," << aabb.maxZ
                << ")]";

      LOG(INFO) << "  contains_edge_bbox=" << (inside ? "YES" : "NO");
    }
  }

  void DebugPrintLSIProfiling(int query_map_id) const {
    auto dbg = readBackStorageBuffer<uint32_t>(prof_counter_buf_, 8);
    auto xcnt = readBackStorageBuffer<uint32_t>(xsect_counter_buf_, 1);

    if (dbg.empty() || xcnt.empty()) {
      LOG(ERROR) << "DebugPrintLSIProfiling: failed to read counters";
      return;
    }

    LOG(INFO) << "LSI DBG:"
              << " query_map_id=" << query_map_id << " raygen=" << (dbg.size() > 0 ? dbg[0] : 0) << " miss=" << (dbg.size() > 1 ? dbg[1] : 0)
              << " intersection_invocations=" << (dbg.size() > 2 ? dbg[2] : 0) << " tested_pairs=" << (dbg.size() > 3 ? dbg[3] : 0)
              << " last_eid=" << (dbg.size() > 4 ? dbg[4] : 0) << " last_prim=" << (dbg.size() > 5 ? dbg[5] : 0)
              << " tested_base_edges=" << (dbg.size() > 6 ? dbg[6] : 0) << " intersections_found=" << xcnt[0];
  }

  void DebugPrintIntersectionsDetailed(int query_map_id, uint32_t max_print = 20) const {
    const int base_map_id = 1 - query_map_id;

    auto query_map = this->ctx_.get_map(query_map_id);
    auto base_map = this->ctx_.get_map(base_map_id);

    if (!query_map || !base_map) {
      LOG(ERROR) << "DebugPrintIntersectionsDetailed: null map";
      return;
    }

    auto xcnt = readBackStorageBuffer<uint32_t>(xsect_counter_buf_, 1);
    if (xcnt.empty()) {
      LOG(ERROR) << "DebugPrintIntersectionsDetailed: failed to read xsect counter";
      return;
    }

    const uint32_t n_xsects = xcnt[0];
    LOG(INFO) << "DebugPrintIntersectionsDetailed:"
              << " query_map_id=" << query_map_id << " base_map_id=" << base_map_id << " intersections_found=" << n_xsects;

    if (n_xsects == 0) return;

    const uint32_t n_print = std::min<uint32_t>(n_xsects, max_print);

    auto gpuXsects = readBackStorageBuffer<xsect_t>(xsect_buf_, n_print);
    auto queryEdges = readBackStorageBuffer<edge_t>(query_map->getEdgesBuffer(), query_map->get_edges_num());
    auto baseEdges = readBackStorageBuffer<edge_t>(base_map->getEdgesBuffer(), base_map->get_edges_num());

    if (gpuXsects.size() != n_print) {
      LOG(ERROR) << "DebugPrintIntersectionsDetailed: failed to read xsect buffer";
      return;
    }

    for (uint32_t i = 0; i < n_print; ++i) {
      const auto &x = gpuXsects[i];

      const uint64_t query_eid = (query_map_id == 0) ? x.eid0 : x.eid1;
      const uint64_t base_eid = (query_map_id == 0) ? x.eid1 : x.eid0;

      if (query_eid >= queryEdges.size() || base_eid >= baseEdges.size()) {
        LOG(ERROR) << "  [" << i << "] invalid eid mapping"
                   << " query_eid=" << query_eid << " base_eid=" << base_eid;
        continue;
      }

      const auto &qe = queryEdges[query_eid];
      const auto &be = baseEdges[base_eid];

      const double q_eval = qe.a * x.x + qe.b * x.y + qe.c;
      const double b_eval = be.a * x.x + be.b * x.y + be.c;

      LOG(INFO) << "  [" << i << "]"
                << " x=(" << x.x << "," << x.y << ")"
                << " query_eid=" << query_eid << " base_eid=" << base_eid << " query_eval=ax+by+c=" << q_eval << " base_eval=ax+by+c=" << b_eval;
    }
  }

  // --------------------------------
  // Intersect Edges
  VkDeviceBuf xsect_buf_{};
  VkDeviceBuf xsect_counter_buf_{};
  VkDeviceBuf prof_counter_buf_{};
  size_t xsect_capacity_ = 0;
};


}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_RT_NS_H
