#ifndef RAYJOIN_MAP_OVERLAY_NATIVE_RT_H
#define RAYJOIN_MAP_OVERLAY_NATIVE_RT_H

#include <memory>

#include "map_overlay_native.h"
#include "query_config.h"
#include "rt/primitive_native.h"
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

  void BuildIndex() override {
    auto& ctx = this->ctx_;
    auto& stream = ctx.get_stream();
    auto win_size = config_.win;
    auto ag_iter = config_.ag_iter;
    auto area_enlarge = config_.enlarge;

    const bool dump_index = rayjoin::ShouldDumpStage(config_.dump_results, "index");
    const std::string index_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_index");

    FOR2 {
      auto d_map = ctx.get_map(im)->DeviceObject();

      if (config_.ag == 0) {
        FillPrimitivesNative(stream, d_map, aabbs_, *eid_range_[im]);
      } else if (config_.ag == 1) {
        FillPrimitivesGroupNewNative(stream, d_map, ag_iter, area_enlarge, aabbs_, *eid_range_[im]);
      } else if (config_.ag == 2) {
        FillPrimitivesGroupNative(stream, d_map, win_size, area_enlarge, aabbs_, *eid_range_[im]);
      } else {
        LOG(FATAL) << "Illegal ag mode: " << config_.ag;
      }

      stream.Sync();

      if (dump_index) {
        DumpIndexResultsCSV(im, index_dir, "native");
      }

      traverse_handles_[im] = rt_engine_->BuildAccelCustom(stream, ArrayView<OptixAabb>(aabbs_));

      stream.Sync();

      if (config_.fau) {
        aabbs_.resize(0);
        aabbs_.shrink_to_fit();
      }
    }
  }

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


  void DumpIndexResultsCSV(int map_id, const std::string& out_dir, const std::string& impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    thrust::host_vector<OptixAabb> h_aabbs = aabbs_;
    thrust::host_vector<thrust::pair<size_t, size_t>> h_ranges = *eid_range_[map_id];

    if (h_aabbs.size() != h_ranges.size()) {
      LOG(ERROR) << "DumpIndexResultsCSV: size mismatch for map_id=" << map_id << " aabbs=" << h_aabbs.size() << " ranges=" << h_ranges.size();
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

    for (size_t i = 0; i < h_aabbs.size(); ++i) {
      const auto& a = h_aabbs[i];
      const auto& r = h_ranges[i];

      ofs << map_id << "," << i << "," << a.minX << "," << a.minY << "," << a.minZ << "," << a.maxX << "," << a.maxY << "," << a.maxZ << ","
          << static_cast<unsigned long long>(r.first) << "," << static_cast<unsigned long long>(r.second) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpIndexResultsCSV: wrote " << path;
  }
};

}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_NATIVE_RT_H
