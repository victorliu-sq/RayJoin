#ifndef RAYJOIN_MAP_OVERLAY_RT_NS_H
#define RAYJOIN_MAP_OVERLAY_RT_NS_H

#include "../engine/vk_buffer_readback.h"
#include "../engine/vk_compute_engine.h"
#include "map_overlay_ns.h"
#include "query_config.h"
#include "vk/core/lsi_rt.h"
#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_rt_engine.h"
#include "vk/map/_NOUSE_lsi_finalize_pass_ns.h"
#include "vk/map/_NOUSE_pip_finalize_pass_ns.h"
#include "vk/map/lsi_rt_pass.h"
#include "vk/map/map.h"
#include "vk/map/pip_rt_pass.h"
#include "vk/rt/as_scene.h"
#include "vk/rt/rt_engine.h"

//////////////////////////////////////////////////
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "util/dump.h"

namespace rayjoin {
namespace vk {

template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
struct Intersection {
  double x;
  double y;

  uint64_t eid0;
  uint64_t eid1;

  uint mid_point_polygon_id = 0;
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

      // PIP debug counters
      pip_debug_counter_buf_[im].Init(sizeof(uint32_t) * 8);

      max_n_points = std::max(max_n_points, np);
      max_n_edges = std::max(max_n_edges, ne);

      // besty
      best_ys_buf_[im].Init(sizeof(double) * np);
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

  // void BuildIndex() override {
  //   auto &ctx = this->ctx_;
  //   auto &vk_ctx = GetVkComputeContext();
  //
  //   auto ag_iter = config_.ag_iter;
  //   auto area_enlarge = config_.enlarge;
  //
  //   const bool dump_index = rayjoin::ShouldDumpStage(config_.dump_results, "index");
  //   const std::string index_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_index");
  //
  //   for (int im = 0; im < 2; im++) {
  //     auto map = ctx.get_map(im);
  //
  //     std::string spvPath = std::string(SHADER_DIR_NS) + "/fill_primitives_ns.spv";
  //
  //     struct FillPrimitivesParams {
  //       uint32_t numEdges;
  //       uint32_t maxIter;
  //       float areaEnlarge;
  //       uint32_t pad;
  //     };
  //
  //     RunComputePass(static_cast<uint32_t>(map_edge_count_[im]),
  //                    spvPath.c_str(),
  //                    FillPrimitivesParams{
  //                        .numEdges = static_cast<uint32_t>(map_edge_count_[im]), .maxIter = ROUNDING_ITER, .areaEnlarge = area_enlarge, .pad = 0},
  //                    map->getPointsBuffer(),
  //                    map->getEdgesBuffer(),
  //                    aabbs_buf_,
  //                    eid_range_buf_[im]);
  //
  //     // DEBUG
  //     // DebugPrintAABBs(map, aabbs_buf_, eid_range_buf_[im], map_edge_count_[im]);
  //
  //     if (dump_index) {
  //       DumpIndexResultsCSV(im, index_dir, "vulkan");
  //     }
  //
  //     LOG(INFO) << "Map-" << im << " builds " << map_edge_count_[im] << " primtives.";
  //     accel_[im].BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);
  //   }
  // }

  void BuildIndex() override {
    auto &ctx = this->ctx_;

    struct FillPrimitivesParams {
      uint32_t numEdges;
      uint32_t maxIter;
      float unused;
      uint32_t pad;
    };

    static_assert(std::is_trivially_copyable_v<FillPrimitivesParams>);
    static_assert(sizeof(FillPrimitivesParams) == 16);

    const bool dump_index = rayjoin::ShouldDumpStage(config_.dump_results, "index");
    const std::string index_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_index");

    for (int im = 0; im < 2; ++im) {
      auto map = ctx.get_map(im);

      // std::string spvPath = std::string(SHADER_DIR_NS) + "/fill_primitives_ns.spv";
      std::string spvPath = std::string(SHADER_KERNEL_NS_DIR) + "/fill_primitives_ns.spv";

      FillPrimitivesParams params{};
      params.numEdges = static_cast<uint32_t>(map_edge_count_[im]);
      params.maxIter = static_cast<uint32_t>(ROUNDING_ITER);
      params.unused = 0.0f;
      params.pad = 0;

      RunComputePass(static_cast<uint32_t>(map_edge_count_[im]),
                     spvPath.c_str(),
                     params,
                     map->getPointsBuffer(),
                     map->getEdgesBuffer(),
                     aabbs_buf_,
                     eid_range_buf_[im]);

      if (dump_index) {
        DumpIndexResultsCSV(im, index_dir, "vulkan");
      }

      LOG(INFO) << "Map-" << im << " builds " << map_edge_count_[im] << " primtives.";
      accel_[im].BuildAccelCustom(aabbs_buf_, static_cast<uint32_t>(map_edge_count_[im]));
    }
  }

  void IntersectEdge(int query_map_id) override {
    const int base_map_id = 1 - query_map_id;

    auto query_map = this->ctx_.get_map(query_map_id);
    auto base_map = this->ctx_.get_map(base_map_id);

    if (!query_map || !base_map) {
      throw std::runtime_error("IntersectEdge(): null map");
    }

    // std::string rgen_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rgen_ns.spv";
    // std::string rint_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rint_ns.spv";
    // std::string rahit_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rahit_ns.spv";
    // std::string rchit_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rchit_ns.spv";
    // std::string rmiss_spv = std::string(SHADER_DIR_NS) + "/rt/lsi_rmiss_ns.spv";

    std::string rgen_spv = std::string(SHADER_RT_NS_DIR) + "/lsi_rgen_ns.spv";
    std::string rint_spv = std::string(SHADER_RT_NS_DIR) + "/lsi_rint_ns.spv";
    std::string rahit_spv = std::string(SHADER_RT_NS_DIR) + "/lsi_rahit_ns.spv";
    std::string rchit_spv = std::string(SHADER_RT_NS_DIR) + "/lsi_rchit_ns.spv";
    std::string rmiss_spv = std::string(SHADER_RT_NS_DIR) + "/lsi_rmiss_ns.spv";

    struct LaunchParamsLSI {
      int32_t query_map_id;
      uint32_t query_edge_count;
      uint32_t xsect_capacity;
      uint32_t _pad0;
    };

    RunRTPass(rgen_spv.c_str(),
              rint_spv.c_str(),
              rahit_spv.c_str(),
              rchit_spv.c_str(),
              rmiss_spv.c_str(),
              accel_[base_map_id].GetTraverseHandle(),
              LaunchParamsLSI{.query_map_id = static_cast<int32_t>(query_map_id),
                              .query_edge_count = static_cast<uint32_t>(query_map->get_edges_num()),
                              .xsect_capacity = static_cast<uint32_t>(xsect_capacity_),
                              ._pad0 = 0u},
              static_cast<uint32_t>(query_map->get_edges_num()),
              base_map->getEdgesBuffer(),  // binding 1 -> gBaseEdges
              base_map->getPointsBuffer(),  // binding 2 -> gBasePoints
              eid_range_buf_[base_map_id],  // binding 3 -> gEidRanges
              query_map->getEdgesBuffer(),  // binding 4 -> gQueryEdges
              query_map->getPointsBuffer(),  // binding 5 -> gQueryPoints
              xsect_buf_,  // binding 6 -> gXsects
              xsect_counter_buf_,  // binding 7 -> gXsectCounter
              prof_counter_buf_);  // binding 8 -> gTestCounter

    // std::string finalize_spv = std::string(SHADER_DIR_NS) + "/lsi_finalize_ns.spv";
    std::string finalize_spv = std::string(SHADER_KERNEL_NS_DIR) + "/lsi_finalize_ns.spv";

    struct LaunchParamsLSIFinalize {
      int32_t query_map_id;
      uint32_t query_edge_count;
      uint32_t xsect_capacity;
      uint32_t _pad0;
    };

    RunComputePass(static_cast<uint32_t>(xsect_capacity_),
                   finalize_spv.c_str(),
                   LaunchParamsLSIFinalize{.query_map_id = query_map_id,
                                           .query_edge_count = static_cast<uint32_t>(query_map->get_edges_num()),
                                           .xsect_capacity = static_cast<uint32_t>(xsect_capacity_),
                                           ._pad0 = 0u},
                   base_map->getEdgesBuffer(),
                   base_map->getPointsBuffer(),
                   query_map->getEdgesBuffer(),
                   query_map->getPointsBuffer(),
                   xsect_buf_,
                   xsect_counter_buf_);

    // this->DebugPrintLSIProfiling(query_map_id);
    // this->DebugPrintIntersectionsDetailed(query_map_id);

    // Test
    // if (rayjoin::ShouldDumpStage(config_.dump_results, "lsi")) {
    //   DumpLSIResultsCSV(query_map_id, rayjoin::DumpSubdir(config_.dump_dir, "results_lsi"), "vulkan");
    // }
    if (query_map_id == 0 && rayjoin::ShouldDumpStage(config_.dump_results, "lsi")) {
      DumpLSIResultsCSV(rayjoin::DumpSubdir(config_.dump_dir, "results_lsi"), "vulkan");
    }
  }

  void LocateVerticesInOtherMap(int query_map_id) override {
    auto &ctx = this->ctx_;
    const int base_map_id = 1 - query_map_id;

    auto query_map = ctx.get_map(query_map_id);
    auto base_map = ctx.get_map(base_map_id);

    if (!query_map || !base_map) {
      throw std::runtime_error("LocateVerticesInOtherMap(): null map");
    }

    // ------------------------------------------------------------
    // RT pass: query point -> closest crossing edge in base map
    // ------------------------------------------------------------
    // std::string rgen_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rgen_ns.spv";
    // std::string rint_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rint_ns.spv";
    // std::string rahit_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rahit_ns.spv";
    // std::string rchit_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rchit_ns.spv";
    // std::string rmiss_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rmiss_ns.spv";
    std::string rgen_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rgen_ns.spv";
    std::string rint_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rint_ns.spv";
    std::string rahit_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rahit_ns.spv";
    std::string rchit_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rchit_ns.spv";
    std::string rmiss_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rmiss_ns.spv";

    struct LaunchParamsPIP {
      int32_t query_map_id;
      uint32_t query_point_count;
      uint32_t _pad0;
      uint32_t _pad1;
    };

    RunRTPass(rgen_spv.c_str(),
              rint_spv.c_str(),
              rahit_spv.c_str(),
              rchit_spv.c_str(),
              rmiss_spv.c_str(),
              accel_[base_map_id].GetTraverseHandle(),
              LaunchParamsPIP{.query_map_id = static_cast<int32_t>(query_map_id),
                              .query_point_count = static_cast<uint32_t>(map_point_count_[query_map_id]),
                              ._pad0 = 0u,
                              ._pad1 = 0u},
              static_cast<uint32_t>(map_point_count_[query_map_id]),
              base_map->getEdgesBuffer(),  // binding 1 -> gBaseEdges
              base_map->getPointsBuffer(),  // binding 2 -> gBasePoints
              eid_range_buf_[base_map_id],  // binding 3 -> gEidRanges
              query_map->getPointsBuffer(),  // binding 4 -> gQueryPoints
              closest_eids_buf_[query_map_id],  // binding 5 -> gClosestEids
              best_ys_buf_[query_map_id],  // binding 6 -> gBestYs
              pip_debug_counter_buf_[query_map_id]);  // binding 7 -> gDebugCounter

    // Debug raw
    this->DebugPrintPIPRawCounters(query_map_id);

    // ------------------------------------------------------------
    // Finalize pass: closest_eid -> polygon_id
    // ------------------------------------------------------------
    // std::string finalize_spv = std::string(SHADER_DIR_NS) + "/pip_finalize_ns.spv";

    // PIPFinalizePassNS finalize_pass(finalize_spv.c_str(),
    //                                 static_cast<uint32_t>(map_point_count_[query_map_id]),
    //                                 static_cast<uint32_t>(EXTERIOR_FACE_ID),
    //                                 base_map->getEdgesBuffer(),
    //                                 base_map->getPointsBuffer(),
    //                                 closest_eids_buf_[query_map_id],
    //                                 point_in_polygon_buf_[query_map_id]);
    //
    // finalize_pass.run();

    // std::string finalize_spv = std::string(SHADER_DIR_NS) + "/pip_finalize_ns.spv";
    std::string finalize_spv = std::string(SHADER_KERNEL_NS_DIR) + "/pip_finalize_ns.spv";

    struct LaunchParamsPIPFinalize {
      uint32_t point_count;
      uint32_t exterior_face_id;
      uint32_t _pad0;
      uint32_t _pad1;
    };

    RunComputePass(static_cast<uint32_t>(map_point_count_[query_map_id]),
                   finalize_spv.c_str(),
                   LaunchParamsPIPFinalize{.point_count = static_cast<uint32_t>(map_point_count_[query_map_id]),
                                           .exterior_face_id = static_cast<uint32_t>(EXTERIOR_FACE_ID),
                                           ._pad0 = 0u,
                                           ._pad1 = 0u},
                   base_map->getEdgesBuffer(),
                   base_map->getPointsBuffer(),
                   closest_eids_buf_[query_map_id],
                   point_in_polygon_buf_[query_map_id]);

    // Compare results from Vulkan and Optix
    // DumpPIPResultsCSV(query_map_id, "tmp/results_pip", "vulkan");

    // optional debug
    // this->DebugPrintPIPProfiling(query_map_id);
    // this->DebugPrintPIPResults(query_map_id);

    // DebugPrintPIPRawCounters(query_map_id);
    if (rayjoin::ShouldDumpStage(config_.dump_results, "pip")) {
      DumpPIPResultsCSV(query_map_id, rayjoin::DumpSubdir(config_.dump_dir, "results_pip"), "vulkan");
    }
  }

  void ComputeOutputPolygons() override {
    using polygon_id_t = index_t;

    auto &ctx = this->ctx_;

    auto xcnt = readBackStorageBuffer<uint32_t>(xsect_counter_buf_, 1);
    if (xcnt.empty()) {
      throw std::runtime_error("ComputeOutputPolygons(): failed to read xsect counter");
    }

    const uint32_t n_xsects = xcnt[0];
    auto xsects_all = readBackStorageBuffer<xsect_t>(xsect_buf_, n_xsects);
    if (xsects_all.size() != n_xsects) {
      throw std::runtime_error("ComputeOutputPolygons(): failed to read xsect buffer");
    }

    for (int im = 0; im < 2; ++im) {
      const int query_map_id = im;
      const int base_map_id = 1 - im;

      auto query_map = ctx.get_map(query_map_id);
      auto base_map = ctx.get_map(base_map_id);

      if (!query_map || !base_map) {
        throw std::runtime_error("ComputeOutputPolygons(): null map");
      }

      auto &xsect_edges_sorted = xsect_edges_sorted_[im];
      xsect_edges_sorted = xsects_all;

      if (xsect_edges_sorted.empty()) {
        continue;
      }

      auto query_edges = readBackStorageBuffer<edge_t>(query_map->getEdgesBuffer(), query_map->get_edges_num());
      auto query_points = readBackStorageBuffer<point_t>(query_map->getPointsBuffer(), query_map->get_points_num());

      auto query_eid_of = [im](const xsect_t &x) -> uint64_t { return (im == 0) ? x.eid0 : x.eid1; };

      auto base_eid_of = [im](const xsect_t &x) -> uint64_t { return (im == 0) ? x.eid1 : x.eid0; };

      std::stable_sort(xsect_edges_sorted.begin(), xsect_edges_sorted.end(), [&](const xsect_t &a, const xsect_t &b) {
        return query_eid_of(a) < query_eid_of(b);
      });

      std::vector<index_t> unique_eids;
      unique_eids.reserve(xsect_edges_sorted.size());

      for (const auto &x: xsect_edges_sorted) {
        const index_t eid = static_cast<index_t>(query_eid_of(x));
        if (unique_eids.empty() || unique_eids.back() != eid) {
          unique_eids.push_back(eid);
        }
      }

      std::vector<uint32_t> xsect_index(unique_eids.size() + 1, 0);
      {
        size_t pos = 0;
        size_t group_idx = 0;
        while (pos < xsect_edges_sorted.size()) {
          const uint64_t eid = query_eid_of(xsect_edges_sorted[pos]);
          size_t end = pos + 1;
          while (end < xsect_edges_sorted.size() && query_eid_of(xsect_edges_sorted[end]) == eid) {
            ++end;
          }
          xsect_index[group_idx + 1] = xsect_index[group_idx] + static_cast<uint32_t>(end - pos);
          pos = end;
          ++group_idx;
        }
      }

      const uint32_t n_mid_points = xsect_index.back() - static_cast<uint32_t>(unique_eids.size());

      struct MidPointTask {
        size_t xsect_idx;
        point_t p;
      };

      std::vector<MidPointTask> tasks;
      tasks.reserve(n_mid_points);

      for (auto &x: xsect_edges_sorted) {
        x.mid_point_polygon_id = std::numeric_limits<polygon_id_t>::max();
      }

      auto dist2_from_edge_start = [&](const xsect_t &x) -> long double {
        const uint64_t eid = query_eid_of(x);
        if (eid >= query_edges.size()) {
          return std::numeric_limits<long double>::infinity();
        }

        const auto &e = query_edges[eid];
        if (e.p1_idx >= query_points.size()) {
          return std::numeric_limits<long double>::infinity();
        }

        const auto &p1 = query_points[e.p1_idx];

        const long double dx = static_cast<long double>(x.x) - static_cast<long double>(p1.x);
        const long double dy = static_cast<long double>(x.y) - static_cast<long double>(p1.y);

        return dx * dx + dy * dy;
      };

      for (size_t group_idx = 0; group_idx < unique_eids.size(); ++group_idx) {
        const size_t begin = xsect_index[group_idx];
        const size_t end = xsect_index[group_idx + 1];
        const size_t n_xsect = end - begin;

        if (n_xsect <= 1) {
          continue;
        }

        struct LocalRef {
          size_t src_idx;
          long double d2;
          uint64_t base_eid;
          long double x;
          long double y;
        };

        std::vector<LocalRef> refs;
        refs.reserve(n_xsect);

        for (size_t i = begin; i < end; ++i) {
          const auto &x = xsect_edges_sorted[i];
          refs.push_back(LocalRef{i, dist2_from_edge_start(x), base_eid_of(x), static_cast<long double>(x.x), static_cast<long double>(x.y)});
        }

        std::stable_sort(refs.begin(), refs.end(), [](const LocalRef &a, const LocalRef &b) {
          if (a.d2 < b.d2) return true;
          if (b.d2 < a.d2) return false;

          if (a.base_eid < b.base_eid) return true;
          if (b.base_eid < a.base_eid) return false;

          if (a.x < b.x) return true;
          if (b.x < a.x) return false;

          if (a.y < b.y) return true;
          if (b.y < a.y) return false;

          return a.src_idx < b.src_idx;
        });

        std::vector<xsect_t> reordered;
        reordered.reserve(n_xsect);
        for (const auto &r: refs) {
          reordered.push_back(xsect_edges_sorted[r.src_idx]);
        }
        for (size_t local_idx = 0; local_idx < n_xsect; ++local_idx) {
          xsect_edges_sorted[begin + local_idx] = reordered[local_idx];
        }

        for (size_t local_idx = 0; local_idx + 1 < n_xsect; ++local_idx) {
          const auto &x1 = xsect_edges_sorted[begin + local_idx];
          const auto &x2 = xsect_edges_sorted[begin + local_idx + 1];

          point_t mp{};
          // mp.x = (x1.x + x2.x) * 0.5;
          // mp.y = (x1.y + x2.y) * 0.5;
          mp.x = x1.x + (x2.x - x1.x) * 0.5;
          mp.y = x1.y + (x2.y - x1.y) * 0.5;

          tasks.push_back(MidPointTask{begin + local_idx, mp});
        }
      }

      if (tasks.empty()) {
        continue;
      }

      std::vector<point_t> host_mid_points;
      host_mid_points.reserve(tasks.size());
      for (const auto &t: tasks) {
        host_mid_points.push_back(t.p);
      }

      if (rayjoin::ShouldDumpStage(config_.dump_results, "pipmid")) {
        const auto out_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_midpoints");
        DumpSortedMidPointsCSV(query_map_id, unique_eids, xsect_index, xsect_edges_sorted, host_mid_points, out_dir, "vulkan");
      }

      VkStagingBuf mid_staging(sizeof(point_t) * host_mid_points.size());
      mid_staging.Host2Stage(host_mid_points);

      VkDeviceBuf mid_points_buf;
      mid_points_buf.Init(sizeof(point_t) * host_mid_points.size());
      mid_staging.Stage2Device(mid_points_buf, sizeof(point_t) * host_mid_points.size());


      // ========================================================================
      // PIP-RT
      std::string rgen_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rgen_ns.spv";
      std::string rint_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rint_ns.spv";
      std::string rahit_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rahit_ns.spv";
      std::string rchit_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rchit_ns.spv";
      std::string rmiss_spv = std::string(SHADER_RT_NS_DIR) + "/pip_rmiss_ns.spv";

      VkDeviceBuf mid_closest_eids_buf;
      mid_closest_eids_buf.Init(sizeof(index_t) * tasks.size());

      VkDeviceBuf mid_best_ys_buf;
      mid_best_ys_buf.Init(sizeof(double) * tasks.size());

      VkDeviceBuf mid_debug_counter_buf;
      mid_debug_counter_buf.Init(sizeof(uint32_t) * 8);

      struct LaunchParamsPIP {
        int32_t query_map_id;
        uint32_t query_point_count;
        uint32_t _pad0;
        uint32_t _pad1;
      };

      RunRTPass(
          rgen_spv.c_str(),
          rint_spv.c_str(),
          rahit_spv.c_str(),
          rchit_spv.c_str(),
          rmiss_spv.c_str(),
          accel_[base_map_id].GetTraverseHandle(),
          LaunchParamsPIP{
              .query_map_id = static_cast<int32_t>(query_map_id), .query_point_count = static_cast<uint32_t>(tasks.size()), ._pad0 = 0u, ._pad1 = 0u},
          static_cast<uint32_t>(tasks.size()),
          base_map->getEdgesBuffer(),  // binding 1 -> gBaseEdges
          base_map->getPointsBuffer(),  // binding 2 -> gBasePoints
          eid_range_buf_[base_map_id],  // binding 3 -> gEidRanges
          mid_points_buf,  // binding 4 -> gQueryPoints
          mid_closest_eids_buf,  // binding 5 -> gClosestEids
          mid_best_ys_buf,  // binding 6 -> gBestYs
          mid_debug_counter_buf);  // binding 7 -> gDebugCounter

      auto mid_closest_eids = readBackStorageBuffer<index_t>(mid_closest_eids_buf, tasks.size());
      if (mid_closest_eids.size() != tasks.size()) {
        throw std::runtime_error("ComputeOutputPolygons(): failed to read midpoint closest eids");
      }

      auto mid_best_ys = readBackStorageBuffer<double>(mid_best_ys_buf, tasks.size());
      if (mid_best_ys.size() != tasks.size()) {
        throw std::runtime_error("ComputeOutputPolygons(): failed to read midpoint best ys");
      }

      if (rayjoin::ShouldDumpStage(config_.dump_results, "pipmid")) {
        DumpMidPointClosestEidsCSV(query_map_id,
                                   unique_eids,
                                   xsect_index,
                                   xsect_edges_sorted,
                                   host_mid_points,
                                   mid_closest_eids,
                                   mid_best_ys,
                                   rayjoin::DumpSubdir(config_.dump_dir, "results_midpoint_closest"),
                                   "vulkan");
      }

      VkDeviceBuf mid_point_in_polygon_buf;
      mid_point_in_polygon_buf.Init(sizeof(index_t) * tasks.size());

      std::string finalize_spv = std::string(SHADER_KERNEL_NS_DIR) + "/pip_finalize_ns.spv";

      struct LaunchParamsPIPFinalize {
        uint32_t point_count;
        uint32_t exterior_face_id;
        uint32_t _pad0;
        uint32_t _pad1;
      };

      RunComputePass(static_cast<uint32_t>(tasks.size()),
                     finalize_spv.c_str(),
                     LaunchParamsPIPFinalize{.point_count = static_cast<uint32_t>(tasks.size()),
                                             .exterior_face_id = static_cast<uint32_t>(EXTERIOR_FACE_ID),
                                             ._pad0 = 0u,
                                             ._pad1 = 0u},
                     base_map->getEdgesBuffer(),
                     base_map->getPointsBuffer(),
                     mid_closest_eids_buf,
                     mid_point_in_polygon_buf);

      auto mid_poly_ids = readBackStorageBuffer<polygon_id_t>(mid_point_in_polygon_buf, tasks.size());
      if (mid_poly_ids.size() != tasks.size()) {
        throw std::runtime_error("ComputeOutputPolygons(): failed to read midpoint polygon ids");
      }

      for (size_t i = 0; i < tasks.size(); ++i) {
        xsect_edges_sorted[tasks[i].xsect_idx].mid_point_polygon_id = static_cast<polygon_id_t>(mid_poly_ids[i]);
      }
    }

    if (rayjoin::ShouldDumpStage(config_.dump_results, "pipmid")) {
      const auto out_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_mid");
      DumpComputeOutputPolygonsCSV(0, out_dir, "vulkan");
      DumpComputeOutputPolygonsCSV(1, out_dir, "vulkan");
    }
  }


  void WriteResult(const std::string &path) override {
    using polygon_id_t = index_t;

    struct OutputChain {
      int64_t id = -1;
      std::vector<point_t> points;
      index_t first_point_idx = 0;
      index_t last_point_idx = 0;
      int64_t left_polygon_id = 0;
      int64_t right_polygon_id = 0;
      int64_t other_map_polygon_id = 0;

      void AddChainPoint(const point_t &p) { points.push_back(p); }

      void AddXsectPoint(const xsect_t &xsect) {
        point_t p{};
        p.x = static_cast<coord_t>(xsect.x);
        p.y = static_cast<coord_t>(xsect.y);
        points.push_back(p);
      }
    };

    struct PointLess {
      bool operator()(const point_t &a, const point_t &b) const {
        if (a.x < b.x) return true;
        if (b.x < a.x) return false;
        return a.y < b.y;
      }
    };

    auto same_point = [](const point_t &a, const point_t &b) { return a.x == b.x && a.y == b.y; };

    std::vector<OutputChain> output_chains;

    auto flush = [&output_chains, &same_point](OutputChain &output_chain) {
      auto &points = output_chain.points;

      if (!points.empty()) {
        if (output_chain.left_polygon_id * output_chain.other_map_polygon_id != 0 ||
            output_chain.right_polygon_id * output_chain.other_map_polygon_id != 0) {
          auto it = std::unique(points.begin(), points.end(), same_point);
          points.resize(static_cast<size_t>(std::distance(points.begin(), it)));
          output_chain.id = static_cast<int64_t>(output_chains.size());
          output_chains.push_back(output_chain);
          points.clear();
        }

        points.clear();
      }
    };

    for (int im = 0; im < 2; ++im) {
      auto poly_ids = readBackStorageBuffer<polygon_id_t>(point_in_polygon_buf_[im], map_point_count_[im]);
      if (poly_ids.size() != map_point_count_[im]) {
        throw std::runtime_error("WriteResult(): failed to read point_in_polygon buffer");
      }

      const auto &xsect_edges_sorted = xsect_edges_sorted_[im];

      auto p_graph_ptr = this->ctx_.get_planar_graph(im);
      CHECK(p_graph_ptr != nullptr) << "planar graph " << im << " is null";
      const auto &p_graph = *p_graph_ptr;

      std::unordered_map<index_t, std::vector<xsect_t>> grouped_xsects;
      grouped_xsects.reserve(xsect_edges_sorted.size());

      for (const auto &xsect: xsect_edges_sorted) {
        const index_t eid = static_cast<index_t>((im == 0) ? xsect.eid0 : xsect.eid1);
        grouped_xsects[eid].push_back(xsect);
      }

      LOG(INFO) << "Map " << im << ", Xsect: " << xsect_edges_sorted.size() << " " << grouped_xsects.size();

      for (size_t ic = 0; ic < p_graph.chains.size(); ++ic) {
        const auto &chain = p_graph.chains[ic];
        const auto begin_pid = p_graph.row_index[ic];
        const auto end_pid = p_graph.row_index[ic + 1];

        OutputChain output_chain;
        output_chain.left_polygon_id = chain.left_polygon_id;
        output_chain.right_polygon_id = chain.right_polygon_id;

        for (auto pid = begin_pid; pid < end_pid; ++pid) {
          output_chain.other_map_polygon_id = poly_ids[pid];
          output_chain.AddChainPoint(p_graph.points[pid]);

          if (pid != end_pid - 1) {
            const auto eid = static_cast<index_t>(pid - ic);
            auto it = grouped_xsects.find(eid);

            if (it != grouped_xsects.end()) {
              auto &xsects = it->second;

              if (!xsects.empty()) {
                output_chain.AddXsectPoint(xsects[0]);

                for (size_t ixsect = 0; ixsect + 1 < xsects.size(); ++ixsect) {
                  flush(output_chain);

                  const auto &xsect = xsects[ixsect];
                  const auto &next_xsect = xsects[ixsect + 1];

                  output_chain.other_map_polygon_id = xsect.mid_point_polygon_id;
                  output_chain.AddXsectPoint(xsect);
                  output_chain.AddXsectPoint(next_xsect);
                }

                flush(output_chain);
                output_chain.AddXsectPoint(xsects.back());
              }
            }
          }
        }

        flush(output_chain);
      }
    }

    std::map<std::pair<int64_t, int64_t>, size_t> face_ids;
    std::map<point_t, index_t, PointLess> point_ids;
    index_t point_counter = 0;

    auto create_polygon = [&](int64_t polygon_id1, int64_t polygon_id2) -> size_t {
      if (polygon_id1 == 0 || polygon_id2 == 0) {
        return 0;
      }

      auto k = std::make_pair(polygon_id1, polygon_id2);
      auto it = face_ids.find(k);
      if (it == face_ids.end()) {
        face_ids[k] = face_ids.size() + 1;
        return face_ids.size();
      }
      return it->second;
    };

    // Jiaxin Patch: fewer faces
    // for (auto &chain: output_chains) {
    //   // Match the original WriteOutputChain semantics:
    //   // map 0 chains created as (map0_poly, map1_poly)
    //   // map 1 chains created as (map0_poly, map1_poly) by reversing order.
    //   //
    //   // Since output_chains contains fragments from both maps, we need the same
    //   // canonicalization the original host writer effectively achieves by input order.
    //   // The safest equivalent is to sort the pair before create_polygon().
    //   {
    //     const int64_t a = chain.left_polygon_id;
    //     const int64_t b = chain.other_map_polygon_id;
    //     chain.left_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
    //   }
    //
    //   {
    //     const int64_t a = chain.right_polygon_id;
    //     const int64_t b = chain.other_map_polygon_id;
    //     chain.right_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
    //   }
    //
    //   for (const auto &p: chain.points) {
    //     if (point_ids.find(p) == point_ids.end()) {
    //       point_ids[p] = point_counter++;
    //     }
    //   }
    //
    //   chain.first_point_idx = point_ids[chain.points.front()];
    //   chain.last_point_idx = point_ids[chain.points.back()];
    // }

    for (auto &chain: output_chains) {
      if (chain.left_polygon_id < chain.other_map_polygon_id) {
        chain.left_polygon_id = create_polygon(chain.left_polygon_id, chain.other_map_polygon_id);
      } else {
        chain.left_polygon_id = create_polygon(chain.other_map_polygon_id, chain.left_polygon_id);
      }

      if (chain.right_polygon_id < chain.other_map_polygon_id) {
        chain.right_polygon_id = create_polygon(chain.right_polygon_id, chain.other_map_polygon_id);
      } else {
        chain.right_polygon_id = create_polygon(chain.other_map_polygon_id, chain.right_polygon_id);
      }

      {
        const int64_t a = chain.left_polygon_id;
        const int64_t b = chain.other_map_polygon_id;
        chain.left_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
      }

      {
        const int64_t a = chain.right_polygon_id;
        const int64_t b = chain.other_map_polygon_id;
        chain.right_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
      }

      for (const auto &p: chain.points) {
        if (point_ids.find(p) == point_ids.end()) {
          point_ids[p] = point_counter++;
        }
      }

      chain.first_point_idx = point_ids[chain.points.front()];
      chain.last_point_idx = point_ids[chain.points.back()];
    }

    LOG(INFO) << "Total chains: " << output_chains.size() << " Total faces: " << face_ids.size();

    std::ofstream ofs(path);
    CHECK(ofs.is_open()) << "Cannot open " << path;
    ofs.setf(std::ios::fixed, std::ios::floatfield);
    ofs.precision(6);

    for (size_t ichain = 0; ichain < output_chains.size(); ++ichain) {
      const auto &chain = output_chains[ichain];
      ofs << (ichain + 1) << " " << chain.points.size() << " " << chain.first_point_idx << " " << chain.last_point_idx << " " << chain.left_polygon_id
          << " " << chain.right_polygon_id << '\n';

      for (const auto &p: chain.points) {
        ofs << p.x << " " << p.y << '\n';
      }
    }

    ofs.close();
  }

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

  // pip_debug
  VkDeviceBuf pip_debug_counter_buf_[2];

  // besty
  std::array<VkDeviceBuf, 2> best_ys_buf_;

  // --------------------------------
  // Build Index
  // std::unique_ptr<FillPrimitivesNS<FillPrimitivesParams>> fill_primitives_ns_pass_;
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

  std::vector<xsect_t> xsect_edges_sorted_[2];

  void DebugPrintPIPRawCounters(int query_map_id) const {
    auto dbg = readBackStorageBuffer<uint32_t>(pip_debug_counter_buf_[query_map_id], 8);

    if (dbg.empty()) {
      LOG(ERROR) << "DebugPrintPIPRawCounters: failed to read pip_debug_counter_buf_"
                 << " for query_map_id=" << query_map_id;
      return;
    }

    LOG(INFO) << "PIP raw dbg:"
              << " query_map_id=" << query_map_id << " raygen=" << (dbg.size() > 0 ? dbg[0] : 0) << " miss=" << (dbg.size() > 1 ? dbg[1] : 0)
              << " intersection_invocations=" << (dbg.size() > 2 ? dbg[2] : 0) << " tested_edges=" << (dbg.size() > 3 ? dbg[3] : 0)
              << " last_point=" << (dbg.size() > 4 ? dbg[4] : 0) << " last_prim=" << (dbg.size() > 5 ? dbg[5] : 0)
              << " reported_hits=" << (dbg.size() > 6 ? dbg[6] : 0) << " accepted_updates=" << (dbg.size() > 7 ? dbg[7] : 0);
  }

  void DumpPIPResultsCSV(int query_map_id, const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;

    auto query_map = this->ctx_.get_map(query_map_id);
    if (!query_map) {
      LOG(ERROR) << "DumpPIPResultsCSV: null query map for query_map_id=" << query_map_id;
      return;
    }

    fs::create_directories(out_dir);

    const uint32_t n_points = static_cast<uint32_t>(map_point_count_[query_map_id]);

    auto closest_eids = readBackStorageBuffer<index_t>(closest_eids_buf_[query_map_id], n_points);
    auto poly_ids = readBackStorageBuffer<index_t>(point_in_polygon_buf_[query_map_id], n_points);

    if (closest_eids.size() != n_points || poly_ids.size() != n_points) {
      LOG(ERROR) << "DumpPIPResultsCSV: failed to read buffers for query_map_id=" << query_map_id << " closest_eids.size=" << closest_eids.size()
                 << " poly_ids.size=" << poly_ids.size() << " expected=" << n_points;
      return;
    }

    const std::string path = out_dir + "/" + impl_tag + "_pip_map_" + std::to_string(query_map_id) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpPIPResultsCSV: failed to open " << path;
      return;
    }

    ofs << "map_id,point_id,closest_eid,poly_id\n";

    const index_t invalid_eid = std::numeric_limits<index_t>::max();

    for (uint32_t point_id = 0; point_id < n_points; ++point_id) {
      const index_t eid = closest_eids[point_id];
      const index_t pid = poly_ids[point_id];

      ofs << query_map_id << "," << point_id << ",";

      if (eid == invalid_eid) {
        ofs << -1;
      } else {
        ofs << static_cast<long long>(eid);
      }

      ofs << "," << static_cast<long long>(pid) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpPIPResultsCSV: wrote " << path;
  }

  void DumpLSIResultsCSV(int query_map_id, const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    auto xcnt = readBackStorageBuffer<uint32_t>(xsect_counter_buf_, 1);
    if (xcnt.empty()) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to read xsect counter";
      return;
    }

    const uint32_t n_xsects = xcnt[0];
    auto gpuXsects = readBackStorageBuffer<xsect_t>(xsect_buf_, n_xsects);

    if (gpuXsects.size() != n_xsects) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to read xsect buffer"
                 << " expected=" << n_xsects << " actual=" << gpuXsects.size();
      return;
    }

    const std::string path = out_dir + "/" + impl_tag + "_lsi_map_" + std::to_string(query_map_id) + ".csv";

    std::vector<std::pair<uint64_t, uint64_t>> pairs;
    pairs.reserve(gpuXsects.size());

    for (const auto &x: gpuXsects) {
      uint64_t eid1 = static_cast<uint64_t>(x.eid0);
      uint64_t eid2 = static_cast<uint64_t>(x.eid1);

      if (eid1 > eid2) {
        std::swap(eid1, eid2);
      }

      pairs.emplace_back(eid1, eid2);
    }

    std::sort(pairs.begin(), pairs.end());

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to open " << path;
      return;
    }

    ofs << "eid1,eid2\n";
    for (const auto &[eid1, eid2]: pairs) {
      ofs << eid1 << "," << eid2 << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpLSIResultsCSV: wrote " << path;
  }

  void DumpLSIResultsCSV(const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    auto xcnt = readBackStorageBuffer<uint32_t>(xsect_counter_buf_, 1);
    if (xcnt.empty()) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to read xsect counter";
      return;
    }

    const uint32_t n_xsects = xcnt[0];
    auto gpuXsects = readBackStorageBuffer<xsect_t>(xsect_buf_, n_xsects);

    if (gpuXsects.size() != n_xsects) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to read xsect buffer"
                 << " expected=" << n_xsects << " actual=" << gpuXsects.size();
      return;
    }

    const std::string path = out_dir + "/" + impl_tag + "_lsi.csv";

    std::vector<std::pair<uint64_t, uint64_t>> pairs;
    pairs.reserve(gpuXsects.size());

    for (const auto &x: gpuXsects) {
      uint64_t eid1 = static_cast<uint64_t>(x.eid0);
      uint64_t eid2 = static_cast<uint64_t>(x.eid1);

      if (eid1 > eid2) {
        std::swap(eid1, eid2);
      }

      pairs.emplace_back(eid1, eid2);
    }

    std::sort(pairs.begin(), pairs.end());

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to open " << path;
      return;
    }

    ofs << "eid1,eid2\n";
    for (const auto &[eid1, eid2]: pairs) {
      ofs << eid1 << "," << eid2 << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpLSIResultsCSV: wrote " << path;
  }

  // void DumpComputeOutputPolygonsCSV(int query_map_id, const std::string &out_dir, const std::string &impl_tag) const {
  //   namespace fs = std::filesystem;
  //   fs::create_directories(out_dir);
  //
  //   const auto &xsects = xsect_edges_sorted_[0];
  //   const std::string path = out_dir + "/" + impl_tag + "_compute_output_polygons_map_0.csv";
  //
  //   std::ofstream ofs(path);
  //   if (!ofs) {
  //     LOG(ERROR) << "DumpComputeOutputPolygonsCSV: failed to open " << path;
  //     return;
  //   }
  //
  //   ofs << "eid1,eid2,mid_point_polygon_id\n";
  //   for (const auto &x: xsects) {
  //     ofs << static_cast<unsigned long long>(x.eid0) << "," << static_cast<unsigned long long>(x.eid1) << ","
  //         << static_cast<uint32_t>(x.mid_point_polygon_id) << "\n";
  //   }
  //
  //   ofs.close();
  //   LOG(INFO) << "DumpComputeOutputPolygonsCSV: wrote " << path;
  // }
  void DumpComputeOutputPolygonsCSV(int query_map_id, const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    if (query_map_id < 0 || query_map_id > 1) {
      LOG(ERROR) << "DumpComputeOutputPolygonsCSV: invalid query_map_id=" << query_map_id;
      return;
    }

    const auto &xsects = xsect_edges_sorted_[query_map_id];
    const std::string path = out_dir + "/" + impl_tag + "_pipmid_map_" + std::to_string(query_map_id) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpComputeOutputPolygonsCSV: failed to open " << path;
      return;
    }

    ofs << "query_map_id,eid_self,eid_other,mid_point_polygon_id\n";
    for (const auto &x: xsects) {
      const uint64_t eid_self = (query_map_id == 0) ? x.eid0 : x.eid1;
      const uint64_t eid_other = (query_map_id == 0) ? x.eid1 : x.eid0;

      ofs << query_map_id << "," << static_cast<unsigned long long>(eid_self) << "," << static_cast<unsigned long long>(eid_other) << ","
          << static_cast<uint32_t>(x.mid_point_polygon_id) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpComputeOutputPolygonsCSV: wrote " << path;
  }

  void DumpSortedMidPointsCSV(int query_map_id,
                              const std::vector<index_t> &unique_eids,
                              const std::vector<uint32_t> &xsect_index,
                              const std::vector<xsect_t> &xsect_edges_sorted,
                              const std::vector<point_t> &mid_points,
                              const std::string &out_dir,
                              const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    const std::string path = out_dir + "/" + impl_tag + "_sorted_midpoints_map_" + std::to_string(query_map_id) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpSortedMidPointsCSV: failed to open " << path;
      return;
    }

    constexpr int kDumpDecimals = 7;
    ofs << std::fixed << std::setprecision(kDumpDecimals);

    ofs << "query_map_id,group_idx,eid_self,local_mid_idx,mid_idx,"
           "eid_other_left,eid_other_right,"
           "x1,y1,x2,y2,mid_x,mid_y\n";

    size_t mid_idx = 0;

    for (size_t group_idx = 0; group_idx < unique_eids.size(); ++group_idx) {
      const uint32_t begin = xsect_index[group_idx];
      const uint32_t end = xsect_index[group_idx + 1];
      const uint32_t n_xsect = end - begin;

      if (n_xsect <= 1) {
        continue;
      }

      const index_t eid_self = unique_eids[group_idx];

      for (uint32_t local_mid_idx = 0; local_mid_idx + 1 < n_xsect; ++local_mid_idx) {
        const uint32_t left_idx = begin + local_mid_idx;
        const uint32_t right_idx = begin + local_mid_idx + 1;

        if (mid_idx >= mid_points.size()) {
          LOG(ERROR) << "DumpSortedMidPointsCSV: midpoint index overflow";
          ofs.close();
          return;
        }

        const auto &x1 = xsect_edges_sorted[left_idx];
        const auto &x2 = xsect_edges_sorted[right_idx];
        const auto &mp = mid_points[mid_idx];

        const index_t eid_other_left = static_cast<index_t>((query_map_id == 0) ? x1.eid1 : x1.eid0);
        const index_t eid_other_right = static_cast<index_t>((query_map_id == 0) ? x2.eid1 : x2.eid0);

        const double x1_dump = TruncateForDump(x1.x, kDumpDecimals);
        const double y1_dump = TruncateForDump(x1.y, kDumpDecimals);
        const double x2_dump = TruncateForDump(x2.x, kDumpDecimals);
        const double y2_dump = TruncateForDump(x2.y, kDumpDecimals);
        const double mx_dump = TruncateForDump(mp.x, kDumpDecimals);
        const double my_dump = TruncateForDump(mp.y, kDumpDecimals);

        ofs << query_map_id << "," << group_idx << "," << static_cast<long long>(eid_self) << "," << local_mid_idx << "," << mid_idx << ","
            << static_cast<long long>(eid_other_left) << "," << static_cast<long long>(eid_other_right) << "," << x1_dump << "," << y1_dump << ","
            << x2_dump << "," << y2_dump << "," << mx_dump << "," << my_dump << "\n";

        ++mid_idx;
      }
    }

    ofs.close();
    LOG(INFO) << "DumpSortedMidPointsCSV: wrote " << path;
  }

  void DumpMidPointClosestEidsCSV(int query_map_id,
                                  const std::vector<index_t> &unique_eids,
                                  const std::vector<uint32_t> &xsect_index,
                                  const std::vector<xsect_t> &xsect_edges_sorted,
                                  const std::vector<point_t> &mid_points,
                                  const std::vector<index_t> &mid_closest_eids,
                                  const std::vector<double> &mid_best_ys,
                                  const std::string &out_dir,
                                  const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    const std::string path = out_dir + "/" + impl_tag + "_midpoint_closest_map_" + std::to_string(query_map_id) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpMidPointClosestEidsCSV: failed to open " << path;
      return;
    }

    constexpr int kDumpDecimals = 7;
    ofs << std::fixed << std::setprecision(kDumpDecimals);
    ofs << "query_map_id,group_idx,eid_self,local_mid_idx,mid_idx,"
           "eid_other_left,eid_other_right,mid_x,mid_y,closest_eid,best_y\n";

    size_t mid_idx = 0;

    for (size_t group_idx = 0; group_idx < unique_eids.size(); ++group_idx) {
      const uint32_t begin = xsect_index[group_idx];
      const uint32_t end = xsect_index[group_idx + 1];
      const uint32_t n_xsect = end - begin;

      if (n_xsect <= 1) {
        continue;
      }

      const index_t eid_self = unique_eids[group_idx];

      for (uint32_t local_mid_idx = 0; local_mid_idx + 1 < n_xsect; ++local_mid_idx) {
        const uint32_t left_idx = begin + local_mid_idx;
        const uint32_t right_idx = begin + local_mid_idx + 1;

        if (mid_idx >= mid_points.size() || mid_idx >= mid_closest_eids.size() || mid_idx >= mid_best_ys.size()) {
          LOG(ERROR) << "DumpMidPointClosestEidsCSV: midpoint index overflow";
          ofs.close();
          return;
        }

        const auto &x1 = xsect_edges_sorted[left_idx];
        const auto &x2 = xsect_edges_sorted[right_idx];
        const auto &mp = mid_points[mid_idx];
        const auto closest_eid = mid_closest_eids[mid_idx];
        const auto best_y = mid_best_ys[mid_idx];

        const index_t eid_other_left = static_cast<index_t>((query_map_id == 0) ? x1.eid1 : x1.eid0);
        const index_t eid_other_right = static_cast<index_t>((query_map_id == 0) ? x2.eid1 : x2.eid0);

        ofs << query_map_id << "," << group_idx << "," << static_cast<long long>(eid_self) << "," << local_mid_idx << "," << mid_idx << ","
            << static_cast<long long>(eid_other_left) << "," << static_cast<long long>(eid_other_right) << "," << TruncateForDump(mp.x, kDumpDecimals)
            << "," << TruncateForDump(mp.y, kDumpDecimals) << ",";

        if (closest_eid == std::numeric_limits<index_t>::max()) {
          ofs << -1;
        } else {
          ofs << static_cast<long long>(closest_eid);
        }

        ofs << "," << TruncateForDump(best_y, kDumpDecimals) << "\n";

        ++mid_idx;
      }
    }

    ofs.close();
    LOG(INFO) << "DumpMidPointClosestEidsCSV: wrote " << path;
  }


  void DumpIndexResultsCSV(int map_id, const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    const uint32_t n = static_cast<uint32_t>(map_edge_count_[map_id]);

    auto h_aabbs = readBackStorageBuffer<VkAabbPositionsKHR>(aabbs_buf_, n);
    auto h_ranges = readBackStorageBuffer<EidRange>(eid_range_buf_[map_id], n);

    if (h_aabbs.size() != n || h_ranges.size() != n) {
      LOG(ERROR) << "DumpIndexResultsCSV: readback size mismatch for map_id=" << map_id << " aabbs=" << h_aabbs.size()
                 << " ranges=" << h_ranges.size() << " expected=" << n;
      return;
    }

    const std::string path = out_dir + "/" + impl_tag + "_index_map_" + std::to_string(map_id) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpIndexResultsCSV: failed to open " << path;
      return;
    }

    ofs << "map_id,primitive_id,min_x,min_y,min_z,max_x,max_y,max_z,eid_begin,eid_end\n";
    ofs << std::fixed << std::setprecision(7);

    for (uint32_t i = 0; i < n; ++i) {
      const auto &a = h_aabbs[i];
      const auto &r = h_ranges[i];

      ofs << map_id << "," << i << "," << a.minX << "," << a.minY << "," << a.minZ << "," << a.maxX << "," << a.maxY << "," << a.maxZ << ","
          << r.first << "," << r.second << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpIndexResultsCSV: wrote " << path;
  }
};


}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_RT_NS_H
