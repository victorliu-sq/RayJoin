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

template<typename CONTEXT_T>
class MapOverlayRT : public MapOverlay<CONTEXT_T> {
  using map_t = typename CONTEXT_T::map_t;

public:
  explicit MapOverlayRT(CONTEXT_T &ctx) : MapOverlay<CONTEXT_T>(ctx) {
    rt_engine_ = std::make_shared<RTEngine>();
    this->lsi_ = std::make_shared<LSIRT<CONTEXT_T>>(ctx, rt_engine_);
    // this->pip_ = std::make_shared<PIPRT<CONTEXT_T>>(ctx, rt_engine_);
  }

  void set_config(const QueryConfigRT &config) { config_ = config; }

  void Init() override {
    auto &ctx = this->ctx_;
    auto &lsi = this->lsi_;
    auto &vk_ctx = GetVkComputeContext();

    // -------------------------------
    // TODO:: Initialize RT Engine
    // -------------------------------
    rt_engine_->Init();

    {
      std::string rgen_spv = std::string(SHADER_DIR) + "/rt/lsi_rgen.spv";
      std::string rint_spv = std::string(SHADER_DIR) + "/rt/lsi_rint.spv";
      std::string rmiss_spv = std::string(SHADER_DIR) + "/rt/lsi_rmiss.spv";

      rt_engine_->InitLSIPipeline(rgen_spv.c_str(), rint_spv.c_str(), rmiss_spv.c_str());
    }
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
      // eid_range_buf_[im].Init(sizeof(std::pair<uint32_t, uint32_t>) * ne);
      eid_range_buf_[im].Init(sizeof(EidRange) * ne);

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
    auto &ctx = this->ctx_;
    auto &vk_ctx = GetVkComputeContext();

    auto ag_iter = config_.ag_iter;
    auto area_enlarge = config_.enlarge;

    for (int im = 0; im < 2; im++) {
      auto map = ctx.get_map(im);

      std::string spvPath = std::string(SHADER_DIR) + "/fill_primitives.spv";

      fill_primitives_pass_ = std::make_unique<FillPrimitives>(spvPath.c_str(),
                                                               map->getPointsBuffer(),
                                                               map->getEdgesBuffer(),
                                                               map->getScalingBuffer(), // ← NEW
                                                               aabbs_buf_,
                                                               eid_range_buf_[im],
                                                               map_edge_count_[im],
                                                               ag_iter,
                                                               area_enlarge);

      fill_primitives_pass_->run();

      DebugPrintAABBs(map, aabbs_buf_, eid_range_buf_[im], map_edge_count_[im]);

      traverse_handles_[im] = rt_engine_->BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);

      // if (config_.fau) {
      //   clearBuffer(aabbs_buf_);
      // }
    }
  }

  void IntersectEdge(int query_map_id) override {
    int base_map_id = 1 - query_map_id;
    auto lsi = std::dynamic_pointer_cast<LSIRT<CONTEXT_T>>(this->lsi_);

    config_.eid_range = &eid_range_buf_[base_map_id];
    config_.handle = traverse_handles_[base_map_id];

    lsi->set_config(config_);
    lsi->Query(query_map_id);

    DebugPrintIntersections(query_map_id);
  }

  void LocateVerticesInOtherMap(int query_map_id) override {}

  void ComputeOutputPolygons() override {}

  void WriteResult(const char *path) override {}

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
  // Queue<xsect_t> xsect_queue_;

  void DebugPrintAABBs(std::shared_ptr<map_t> map, const VkDeviceBuf &aabbBuf, const VkDeviceBuf &eidRangeBuf, uint32_t edge_count) const {
    uint32_t checkCount = std::min<uint32_t>(edge_count, 10);

    auto gpuAABBs = readBackStorageBuffer<VkAabbPositionsKHR>(aabbBuf, checkCount);

    auto gpuRanges = readBackStorageBuffer<EidRange>(eidRangeBuf, checkCount);

    auto gpuEdges = readBackStorageBuffer<Edge>(map->getEdgesBuffer(), edge_count);

    auto gpuPts = readBackStorageBuffer<DstPointI64>(map->getPointsBuffer(), map->get_points_num());

    auto scaling = readBackStorageBuffer<Scaling<double, int64_t>>(map->getScalingBuffer(), 1)[0];

    LOG(INFO) << "Debug AABB validation (first " << checkCount << " primitives):";

    for (uint32_t i = 0; i < checkCount; i++) {
      const auto &aabb = gpuAABBs[i];
      const auto &range = gpuRanges[i];

      uint32_t eid = range.first;

      const auto &e = gpuEdges[eid];

      auto &p1 = gpuPts[e.p1_idx];
      auto &p2 = gpuPts[e.p2_idx];

      double x1 = scaling.UnscaleX(p1.x);
      double y1 = scaling.UnscaleY(p1.y);

      double x2 = scaling.UnscaleX(p2.x);
      double y2 = scaling.UnscaleY(p2.y);

      double minx = std::min(x1, x2);
      double maxx = std::max(x1, x2);
      double miny = std::min(y1, y2);
      double maxy = std::max(y1, y2);

      bool inside = (minx >= aabb.minX && maxx <= aabb.maxX && miny >= aabb.minY && maxy <= aabb.maxY);

      LOG(INFO) << "AABB=[("
          << aabb.minX << "," << aabb.minY << "," << aabb.minZ << ") ("
          << aabb.maxX << "," << aabb.maxY << "," << aabb.maxZ << ")]";

      LOG(INFO) << "eid=" << eid << " edge=(" << x1 << "," << y1 << ") -> (" << x2 << "," << y2 << ")"
                << " AABB=[(" << aabb.minX << "," << aabb.minY << ") (" << aabb.maxX << "," << aabb.maxY << ")]"
                << " contains=" << (inside ? "YES" : "NO");
    }
  }

private:
  void DebugPrintIntersections(int query_map_id, uint32_t max_print = 20) const {
    using lsi_t = LSIRT<CONTEXT_T>;

    auto lsi = std::dynamic_pointer_cast<lsi_t>(this->lsi_);
    if (!lsi) {
      LOG(ERROR) << "DebugPrintIntersections: failed to cast lsi_ to LSIRT";
      return;
    }

    int base_map_id = 1 - query_map_id;
    auto query_map = this->ctx_.get_map(query_map_id);
    auto base_map = this->ctx_.get_map(base_map_id);

    if (!query_map || !base_map) {
      LOG(ERROR) << "DebugPrintIntersections: null map";
      return;
    }

    // ------------------------------------------------------------------------
    // Read back intersection count from GPU
    // ------------------------------------------------------------------------
    auto xsectCountVec = readBackStorageBuffer<uint32_t>(lsi->get_xsect_counter_buffer(), 1);

    if (xsectCountVec.empty()) {
      LOG(ERROR) << "DebugPrintIntersections: failed to read xsect counter";
      return;
    }

    uint32_t n_xsects = xsectCountVec[0];
    LOG(INFO) << "DebugPrintIntersections: query_map_id=" << query_map_id << ", base_map_id=" << base_map_id
              << ", gpu intersection count=" << n_xsects;

    auto dbg = readBackStorageBuffer<uint32_t>(lsi->get_prof_counter_buffer(), 8);
    if (!dbg.empty()) {
      LOG(INFO) << "LSI DBG:"
                << " raygen=" << dbg[0] << " miss=" << (dbg.size() > 1 ? dbg[1] : 0) << " inter=" << (dbg.size() > 2 ? dbg[2] : 0)
                << " writes=" << (dbg.size() > 3 ? dbg[3] : 0) << " last_eid=" << (dbg.size() > 4 ? dbg[4] : 0)
                << " last_prim=" << (dbg.size() > 5 ? dbg[5] : 0);
    }

    if (n_xsects == 0) {
      LOG(INFO) << "DebugPrintIntersections: no intersections reported";
      return;
    }

    uint32_t n_print = std::min<uint32_t>(n_xsects, max_print);

    // ------------------------------------------------------------------------
    // Read back first N intersections
    // ------------------------------------------------------------------------
    auto gpuXsects = readBackStorageBuffer<Intersection>(lsi->get_xsect_buffer(), n_print);

    if (gpuXsects.size() != n_print) {
      LOG(ERROR) << "DebugPrintIntersections: failed to read xsect buffer";
      return;
    }

    // ------------------------------------------------------------------------
    // Read back both maps' edges/points/scaling
    // ------------------------------------------------------------------------
    auto queryEdges = readBackStorageBuffer<Edge>(query_map->getEdgesBuffer(), query_map->get_edges_num());
    auto baseEdges = readBackStorageBuffer<Edge>(base_map->getEdgesBuffer(), base_map->get_edges_num());

    auto queryPts = readBackStorageBuffer<DstPointI64>(query_map->getPointsBuffer(), query_map->get_points_num());
    auto basePts = readBackStorageBuffer<DstPointI64>(base_map->getPointsBuffer(), base_map->get_points_num());

    auto queryScaling = readBackStorageBuffer<Scaling<double, int64_t>>(query_map->getScalingBuffer(), 1)[0];

    auto baseScaling = readBackStorageBuffer<Scaling<double, int64_t>>(base_map->getScalingBuffer(), 1)[0];

    // ------------------------------------------------------------------------
    // Small local CPU checker that mirrors the sign-side logic
    // ------------------------------------------------------------------------
    auto subedge = [](const DstPointI64 &p, const Edge &e) -> long long {
      return static_cast<long long>(p.x) * static_cast<long long>(e.a) + static_cast<long long>(p.y) * static_cast<long long>(e.b) + e.c;
    };

    auto cpu_intersect_test =
        [&](const Edge &e1, const DstPointI64 &e1_p1, const DstPointI64 &e1_p2, const Edge &e2, const DstPointI64 &e2_p1, const DstPointI64 &e2_p2)
        -> bool {
      auto e2_p1_agst_e1 = subedge(e2_p1, e1);
      auto e2_p2_agst_e1 = subedge(e2_p2, e1);
      auto e1_p1_agst_e2 = subedge(e1_p1, e2);
      auto e1_p2_agst_e2 = subedge(e1_p2, e2);

      if (e1_p1_agst_e2 == 0)
        e1_p1_agst_e2 = -e2.a;
      if (e1_p1_agst_e2 == 0)
        e1_p1_agst_e2 = -e2.b;
      if (e1_p1_agst_e2 == 0)
        return false;

      if (e1_p2_agst_e2 == 0)
        e1_p2_agst_e2 = -e2.a;
      if (e1_p2_agst_e2 == 0)
        e1_p2_agst_e2 = -e2.b;
      if (e1_p2_agst_e2 == 0)
        return false;

      if ((e1_p1_agst_e2 > 0 && e1_p2_agst_e2 > 0) || (e1_p1_agst_e2 < 0 && e1_p2_agst_e2 < 0)) {
        return false;
      }

      if (e2_p1_agst_e1 == 0)
        e2_p1_agst_e1 = e1.a;
      if (e2_p1_agst_e1 == 0)
        e2_p1_agst_e1 = e1.b;
      if (e2_p1_agst_e1 == 0)
        return false;

      if (e2_p2_agst_e1 == 0)
        e2_p2_agst_e1 = e1.a;
      if (e2_p2_agst_e1 == 0)
        e2_p2_agst_e1 = e1.b;
      if (e2_p2_agst_e1 == 0)
        return false;

      if ((e2_p1_agst_e1 > 0 && e2_p2_agst_e1 > 0) || (e2_p1_agst_e1 < 0 && e2_p2_agst_e1 < 0)) {
        return false;
      }

      bool same_dir = (e1_p1.x == e2_p1.x && e1_p1.y == e2_p1.y && e1_p2.x == e2_p2.x && e1_p2.y == e2_p2.y);

      bool opp_dir = (e1_p1.x == e2_p2.x && e1_p1.y == e2_p2.y && e1_p2.x == e2_p1.x && e1_p2.y == e2_p1.y);

      if (same_dir || opp_dir)
        return false;

      return true;
    };

    LOG(INFO) << "DebugPrintIntersections: printing first " << n_print << " intersections";

    for (uint32_t i = 0; i < n_print; ++i) {
      const auto &x = gpuXsects[i];

      // ----------------------------------------------------------------------
      // IMPORTANT:
      // Adjust these two lines if your Intersection struct uses different field
      // names. The logic assumes eid0/eid1 correspond to map0/map1.
      // ----------------------------------------------------------------------
      uint32_t query_eid = (query_map_id == 0) ? x.eid0 : x.eid1;
      uint32_t base_eid = (query_map_id == 0) ? x.eid1 : x.eid0;

      bool eid_ok = (query_eid < queryEdges.size()) && (base_eid < baseEdges.size());

      if (!eid_ok) {
        LOG(ERROR) << "  [" << i << "] invalid eids"
                   << " query_eid=" << query_eid << " base_eid=" << base_eid << " queryEdges.size=" << queryEdges.size()
                   << " baseEdges.size=" << baseEdges.size();
        continue;
      }

      const auto &qe = queryEdges[query_eid];
      const auto &be = baseEdges[base_eid];

      const auto &qp1 = queryPts[qe.p1_idx];
      const auto &qp2 = queryPts[qe.p2_idx];
      const auto &bp1 = basePts[be.p1_idx];
      const auto &bp2 = basePts[be.p2_idx];

      bool cpu_ok = cpu_intersect_test(qe, qp1, qp2, be, bp1, bp2);

      double qx1 = queryScaling.UnscaleX(qp1.x);
      double qy1 = queryScaling.UnscaleY(qp1.y);
      double qx2 = queryScaling.UnscaleX(qp2.x);
      double qy2 = queryScaling.UnscaleY(qp2.y);

      double bx1 = baseScaling.UnscaleX(bp1.x);
      double by1 = baseScaling.UnscaleY(bp1.y);
      double bx2 = baseScaling.UnscaleX(bp2.x);
      double by2 = baseScaling.UnscaleY(bp2.y);

      LOG(INFO) << "  [" << i << "]"
                << " query_eid=" << query_eid << " base_eid=" << base_eid << " cpu_intersect=" << (cpu_ok ? "YES" : "NO") << " | query=(" << qx1
                << "," << qy1 << ") -> (" << qx2 << "," << qy2 << ")"
                << " | base=(" << bx1 << "," << by1 << ") -> (" << bx2 << "," << by2 << ")";
    }
  }
};

} // namespace vk
} // namespace rayjoin
#endif // RAYJOIN_MAP_OVERLAY_RT_H
