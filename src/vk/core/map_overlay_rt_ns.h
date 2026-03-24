#ifndef RAYJOIN_MAP_OVERLAY_RT_NS_H
#define RAYJOIN_MAP_OVERLAY_RT_NS_H

#include "map_overlay_ns.h"
#include "query_config.h"
#include "vk/core/lsi_rt.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/lsi_finalize_pass_ns.h"
#include "vk/map/lsi_rt_pass.h"
#include "vk/map/map.h"
#include "vk/map/pip_finalize_pass_ns.h"
#include "vk/map/pip_rt_pass.h"
#include "vk/map/vk_debug_readback.h"
#include "vk/rt/as_scene.h"
#include "vk/rt/primitive_ns.h"
#include "vk/rt/rt_engine.h"

//////////////////////////////////////////////////
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace rayjoin {
namespace vk {

template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
struct Intersection {
  double x;
  double y;

  uint64_t eid0;
  uint64_t eid1;

  // default the mid_point_poly_id to 0
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

    // Test
    DumpLSIResultsCSV(query_map_id, "tmp/results_lsi", "vulkan");
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
    std::string rgen_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rgen_ns.spv";
    std::string rint_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rint_ns.spv";
    std::string rahit_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rahit_ns.spv";
    std::string rchit_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rchit_ns.spv";
    std::string rmiss_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rmiss_ns.spv";

    PIPRTPassNS rt_pass(rgen_spv.c_str(),
                        rint_spv.c_str(),
                        rahit_spv.c_str(),
                        rchit_spv.c_str(),
                        rmiss_spv.c_str(),
                        accel_[base_map_id].GetTraverseHandle(),
                        eid_range_buf_[base_map_id],
                        base_map->getPointsBuffer(),
                        base_map->getEdgesBuffer(),
                        query_map->getPointsBuffer(),
                        closest_eids_buf_[query_map_id],
                        pip_debug_counter_buf_[query_map_id],
                        static_cast<uint32_t>(query_map_id),
                        static_cast<uint32_t>(map_point_count_[query_map_id]));

    rt_pass.run();

    this->DebugPrintPIPRawCounters(query_map_id);

    // ------------------------------------------------------------
    // Finalize pass: closest_eid -> polygon_id
    // ------------------------------------------------------------
    std::string finalize_spv = std::string(SHADER_DIR_NS) + "/pip_finalize_ns.spv";

    PIPFinalizePassNS finalize_pass(finalize_spv.c_str(),
                                    static_cast<uint32_t>(map_point_count_[query_map_id]),
                                    static_cast<uint32_t>(EXTERIOR_FACE_ID),
                                    base_map->getEdgesBuffer(),
                                    base_map->getPointsBuffer(),
                                    closest_eids_buf_[query_map_id],
                                    point_in_polygon_buf_[query_map_id]);

    finalize_pass.run();

    // Compare results from Vulkan and Optix
    // DumpPIPResultsCSV(query_map_id, "tmp/results_pip", "vulkan");

    // optional debug
    // this->DebugPrintPIPProfiling(query_map_id);
    // this->DebugPrintPIPResults(query_map_id);

    // DebugPrintPIPRawCounters(query_map_id);
  }
  void ComputeOutputPolygons() override {
    using polygon_id_t = index_t;

    auto &ctx = this->ctx_;

    constexpr int im = 0;
    const int query_map_id = 0;
    const int base_map_id = 1;

    auto query_map = ctx.get_map(query_map_id);
    auto base_map = ctx.get_map(base_map_id);

    if (!query_map || !base_map) {
      throw std::runtime_error("ComputeOutputPolygons(): null map");
    }

    // Read all intersections currently in the shared xsect buffer.
    auto xcnt = readBackStorageBuffer<uint32_t>(xsect_counter_buf_, 1);
    if (xcnt.empty()) {
      throw std::runtime_error("ComputeOutputPolygons(): failed to read xsect counter");
    }

    const uint32_t n_xsects = xcnt[0];

    // Cache full intersection list for map 0 only.
    auto &xsect_edges_sorted = xsect_edges_sorted_[0];
    xsect_edges_sorted = readBackStorageBuffer<xsect_t>(xsect_buf_, n_xsects);

    if (xsect_edges_sorted.size() != n_xsects) {
      throw std::runtime_error("ComputeOutputPolygons(): failed to read xsect buffer");
    }

    if (n_xsects == 0) {
      return;
    }

    // Sort by eid0, then by distance from edge start point.
    auto query_edges = readBackStorageBuffer<edge_t>(query_map->getEdgesBuffer(), query_map->get_edges_num());
    auto query_points = readBackStorageBuffer<point_t>(query_map->getPointsBuffer(), query_map->get_points_num());

    auto eid_of = [](const xsect_t &x) -> uint64_t { return x.eid0; };

    auto dist2_from_edge_start = [&](const xsect_t &x) -> double {
      const uint64_t eid = eid_of(x);
      if (eid >= query_edges.size()) {
        return std::numeric_limits<double>::infinity();
      }

      const auto &e = query_edges[eid];
      if (e.p1_idx >= query_points.size()) {
        return std::numeric_limits<double>::infinity();
      }

      const auto &p1 = query_points[e.p1_idx];
      const double dx = x.x - static_cast<double>(p1.x);
      const double dy = x.y - static_cast<double>(p1.y);
      return dx * dx + dy * dy;
    };

    std::sort(xsect_edges_sorted.begin(), xsect_edges_sorted.end(), [&](const xsect_t &a, const xsect_t &b) {
      const uint64_t ea = eid_of(a);
      const uint64_t eb = eid_of(b);
      if (ea != eb) return ea < eb;
      return dist2_from_edge_start(a) < dist2_from_edge_start(b);
    });

    struct MidPointTask {
      size_t xsect_idx;
      point_t p;
    };

    std::vector<MidPointTask> tasks;
    tasks.reserve(n_xsects);

    // Build midpoint list for every consecutive pair on the same edge.
    size_t begin = 0;
    while (begin < xsect_edges_sorted.size()) {
      const uint64_t eid = eid_of(xsect_edges_sorted[begin]);
      size_t end = begin + 1;
      while (end < xsect_edges_sorted.size() && eid_of(xsect_edges_sorted[end]) == eid) {
        ++end;
      }

      const size_t count = end - begin;
      if (count > 1) {
        for (size_t i = begin; i + 1 < end; ++i) {
          const auto &x1 = xsect_edges_sorted[i];
          const auto &x2 = xsect_edges_sorted[i + 1];

          point_t mp{};
          mp.x = static_cast<coord_t>(x1.x + (x2.x - x1.x) * 0.5);
          mp.y = static_cast<coord_t>(x1.y + (x2.y - x1.y) * 0.5);

          tasks.push_back(MidPointTask{i, mp});
        }
      }

      begin = end;
    }

    if (tasks.empty()) {
      return;
    }

    // Upload midpoints to GPU.
    VkStagingBuf mid_staging(sizeof(point_t) * tasks.size());
    {
      std::vector<point_t> host_mid_points;
      host_mid_points.reserve(tasks.size());
      for (const auto &t: tasks) {
        host_mid_points.push_back(t.p);
      }
      mid_staging.Host2Stage(host_mid_points);
    }

    VkDeviceBuf mid_points_buf;
    mid_points_buf.Init(sizeof(point_t) * tasks.size());
    mid_staging.Stage2Device(mid_points_buf, sizeof(point_t) * tasks.size());

    // Reuse existing PIP RT pass to query midpoint containment in map 1.
    std::string rgen_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rgen_ns.spv";
    std::string rint_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rint_ns.spv";
    std::string rahit_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rahit_ns.spv";
    std::string rchit_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rchit_ns.spv";
    std::string rmiss_spv = std::string(SHADER_DIR_NS) + "/rt/pip_rmiss_ns.spv";

    VkDeviceBuf mid_closest_eids_buf;
    mid_closest_eids_buf.Init(sizeof(index_t) * tasks.size());

    VkDeviceBuf mid_debug_counter_buf;
    mid_debug_counter_buf.Init(sizeof(uint32_t) * 8);

    PIPRTPassNS rt_pass(rgen_spv.c_str(),
                        rint_spv.c_str(),
                        rahit_spv.c_str(),
                        rchit_spv.c_str(),
                        rmiss_spv.c_str(),
                        accel_[base_map_id].GetTraverseHandle(),
                        eid_range_buf_[base_map_id],
                        base_map->getPointsBuffer(),
                        base_map->getEdgesBuffer(),
                        mid_points_buf,
                        mid_closest_eids_buf,
                        mid_debug_counter_buf,
                        static_cast<uint32_t>(query_map_id),
                        static_cast<uint32_t>(tasks.size()));

    rt_pass.run();

    // Finalize midpoint closest eid -> polygon id.
    VkDeviceBuf mid_point_in_polygon_buf;
    mid_point_in_polygon_buf.Init(sizeof(index_t) * tasks.size());

    std::string finalize_spv = std::string(SHADER_DIR_NS) + "/pip_finalize_ns.spv";
    PIPFinalizePassNS finalize_pass(finalize_spv.c_str(),
                                    static_cast<uint32_t>(tasks.size()),
                                    static_cast<uint32_t>(EXTERIOR_FACE_ID),
                                    base_map->getEdgesBuffer(),
                                    base_map->getPointsBuffer(),
                                    mid_closest_eids_buf,
                                    mid_point_in_polygon_buf);

    finalize_pass.run();

    auto mid_poly_ids = readBackStorageBuffer<polygon_id_t>(mid_point_in_polygon_buf, tasks.size());
    if (mid_poly_ids.size() != tasks.size()) {
      throw std::runtime_error("ComputeOutputPolygons(): failed to read midpoint polygon ids");
    }

    // Fill mid_point_polygon_id into the first xsect of every consecutive pair.
    for (size_t i = 0; i < tasks.size(); ++i) {
      xsect_edges_sorted[tasks[i].xsect_idx].mid_point_polygon_id = static_cast<uint32_t>(mid_poly_ids[i]);
    }


    // Check for correctness of this method
    DumpComputeOutputPolygonsCSV(0, "tmp/results_compute_output_polygons", "vulkan");
  }

  void WriteResult(const char *path) override {
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
      auto &pts = output_chain.points;

      if (!pts.empty()) {
        if (output_chain.left_polygon_id * output_chain.other_map_polygon_id != 0 ||
            output_chain.right_polygon_id * output_chain.other_map_polygon_id != 0) {
          auto it = std::unique(pts.begin(), pts.end(), same_point);
          pts.resize(static_cast<size_t>(std::distance(pts.begin(), it)));
          output_chain.id = static_cast<int64_t>(output_chains.size());
          output_chains.push_back(output_chain);
        }

        pts.clear();
      }
    };

    constexpr int im = 0;

    auto poly_ids = readBackStorageBuffer<polygon_id_t>(point_in_polygon_buf_[0], map_point_count_[0]);
    if (poly_ids.size() != map_point_count_[0]) {
      throw std::runtime_error("WriteResult(): failed to read point_in_polygon buffer");
    }

    const auto &xsect_edges_sorted = xsect_edges_sorted_[0];
    const auto &p_graph = *this->ctx_.get_planar_graph(0);

    std::unordered_map<index_t, std::vector<xsect_t>> grouped_xsects;
    grouped_xsects.reserve(xsect_edges_sorted.size());

    for (const auto &x: xsect_edges_sorted) {
      grouped_xsects[static_cast<index_t>(x.eid0)].push_back(x);
    }

    LOG(INFO) << "Map 0, Xsect: " << xsect_edges_sorted.size() << " " << grouped_xsects.size();

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

              for (size_t ix = 0; ix + 1 < xsects.size(); ++ix) {
                flush(output_chain);

                const auto &x1 = xsects[ix];
                const auto &x2 = xsects[ix + 1];

                output_chain.other_map_polygon_id = x1.mid_point_polygon_id;
                output_chain.AddXsectPoint(x1);
                output_chain.AddXsectPoint(x2);
              }

              flush(output_chain);
              output_chain.AddXsectPoint(xsects.back());
            }
          }
        }
      }

      flush(output_chain);
    }

    std::map<std::pair<int64_t, int64_t>, size_t> face_ids;
    std::map<point_t, index_t, PointLess> point_ids;
    index_t point_counter = 0;

    auto create_polygon = [&face_ids](int64_t polygon_id1, int64_t polygon_id2) -> size_t {
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

  void DumpComputeOutputPolygonsCSV(int query_map_id, const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    if (query_map_id != 0) {
      LOG(INFO) << "DumpComputeOutputPolygonsCSV: skip query_map_id=" << query_map_id;
      return;
    }

    const auto &xsects = xsect_edges_sorted_[0];
    const std::string path = out_dir + "/" + impl_tag + "_compute_output_polygons_map_0.csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpComputeOutputPolygonsCSV: failed to open " << path;
      return;
    }

    ofs << "eid1,eid2,mid_point_polygon_id\n";
    for (const auto &x: xsects) {
      ofs << static_cast<unsigned long long>(x.eid0) << "," << static_cast<unsigned long long>(x.eid1) << ","
          << static_cast<unsigned long long>(x.mid_point_polygon_id) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpComputeOutputPolygonsCSV: wrote " << path;
  }
};


}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_RT_NS_H
