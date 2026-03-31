#ifndef RAYJOIN_MAP_OVERLAY_RT_H
#define RAYJOIN_MAP_OVERLAY_RT_H

#include "../engine/vk_buffer_readback.h"
#include "vk/core/lsi_rt.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/map.h"
#include "vk/rt/primitives.h"
#include "vk/rt/rt_engine.h"

namespace rayjoin {
namespace vk {

template<typename CONTEXT_T>
class MapOverlayRT : public MapOverlay<CONTEXT_T> {
  using map_t = typename CONTEXT_T::map_t;

 public:
  explicit MapOverlayRT(CONTEXT_T &ctx) : MapOverlay<CONTEXT_T>(ctx) {}

  void set_config(const QueryConfigRT &config) { config_ = config; }

  void Init() override {
    auto &ctx = this->ctx_;
    // auto &lsi = this->lsi_;
    auto &vk_ctx = GetVkComputeContext();
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
    (void) n_edges;
    (void) vk_ctx;
  }

  // void BuildIndex() override {
  //   auto &ctx = this->ctx_;
  //   auto &vk_ctx = GetVkComputeContext();
  //
  //   auto ag_iter = config_.ag_iter;
  //   auto area_enlarge = config_.enlarge;
  //
  //   for (int im = 0; im < 2; im++) {
  //     auto map = ctx.get_map(im);
  //
  //     std::string spvPath = std::string(SHADER_DIR) + "/fill_primitives.spv";
  //
  //     fill_primitives_pass_ = std::make_unique<FillPrimitives>(spvPath.c_str(),
  //                                                              map->getPointsBuffer(),
  //                                                              map->getEdgesBuffer(),
  //                                                              map->getScalingBuffer(),  // ← NEW
  //                                                              aabbs_buf_,
  //                                                              eid_range_buf_[im],
  //                                                              map_edge_count_[im],
  //                                                              ag_iter,
  //                                                              area_enlarge);
  //     fill_primitives_pass_->run();
  //
  //     // DebugPrintAABBs(map, aabbs_buf_, eid_range_buf_[im], map_edge_count_[im]);
  //
  //     LOG(INFO) << "Map-" << im << " builds " << map_edge_count_[im] << " primtives.";
  //     // traverse_handles_[im] = rt_engine_->BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);
  //     // traverse_handles_[im] = rt_engine_->BuildAccelCustom(aabbs_buf_, map_edge_count_[im]);
  //
  //     // if (config_.fau) {
  //     //   clearBuffer(aabbs_buf_);
  //     // }
  //   }
  // }

  void BuildIndex() override {
    auto &ctx = this->ctx_;

    struct FillPrimitivesParams {
      uint32_t numEdges;
      uint32_t maxIter;
      float areaEnlarge;
      uint32_t pad;
    };

    static_assert(std::is_trivially_copyable_v<FillPrimitivesParams>);
    static_assert(sizeof(FillPrimitivesParams) == 16);

    const bool dump_index = rayjoin::ShouldDumpStage(config_.dump_results, "index");
    const std::string index_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_index");

    for (int im = 0; im < 2; ++im) {
      auto map = ctx.get_map(im);

      std::string spvPath = std::string(SHADER_DIR) + "/fill_primitives.spv";

      FillPrimitivesParams params{};
      params.numEdges = static_cast<uint32_t>(map_edge_count_[im]);
      params.maxIter = static_cast<uint32_t>(ROUNDING_ITER);
      params.areaEnlarge = config_.enlarge;
      params.pad = 0;

      RunComputePass(static_cast<uint32_t>(map_edge_count_[im]),
                     spvPath.c_str(),
                     params,
                     map->getPointsBuffer(),
                     map->getEdgesBuffer(),
                     aabbs_buf_,
                     eid_range_buf_[im],
                     map->getScalingBuffer());

      LOG(INFO) << "Map-" << im << " builds " << map_edge_count_[im] << " primtives.";

      if (dump_index) {
        DumpIndexResultsCSV(im, index_dir, "vulkan");
      }

      accel_[im].BuildAccelCustom(aabbs_buf_, static_cast<uint32_t>(map_edge_count_[im]));
      traverse_handles_[im] = accel_[im].GetTraverseHandle();
    }
  }

  void IntersectEdge(int query_map_id) override {
    int base_map_id = 1 - query_map_id;
    // auto lsi = std::dynamic_pointer_cast<LSIRT<CONTEXT_T>>(this->lsi_);

    config_.eid_range = &eid_range_buf_[base_map_id];
    config_.handle = traverse_handles_[base_map_id];

    // lsi->set_config(config_);
    // lsi->Query(query_map_id);

    // DebugPrintIntersectionsDetailed(query_map_id);
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


  AccelStructScene accel_[2];

  // -------------------------------
  // BVH Handlers
  // -------------------------------
  VkAccelerationStructureKHR traverse_handles_[2];
  // Queue<xsect_t> xsect_queue_;

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
    ofs << std::fixed << std::setprecision(9);

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
#endif  // RAYJOIN_MAP_OVERLAY_RT_H
