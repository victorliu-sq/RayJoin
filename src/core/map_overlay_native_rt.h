#ifndef RAYJOIN_MAP_OVERLAY_NATIVE_RT_H
#define RAYJOIN_MAP_OVERLAY_NATIVE_RT_H

#include <memory>
#include <thrust/binary_search.h>
#include <thrust/distance.h>

#include "lsi_rt_native.h"
#include "map_overlay_native.h"
#include "pip_rt_natve.h"
#include "query_config.h"
#include "rt/primitive_native.h"
#include "rt/rt_engine.h"

namespace rayjoin {

template<typename CONTEXT_T>
class MapOverlayNativeRT : public MapOverlayNative<CONTEXT_T> {
 public:
  using map_t = typename CONTEXT_T::map_t;
  using point_t = typename map_t::point_t;
  using edge_t = typename map_t::edge_t;
  using xsect_t = typename LSINative<CONTEXT_T>::xsect_t;

  explicit MapOverlayNativeRT(CONTEXT_T& ctx) : MapOverlayNative<CONTEXT_T>(ctx) {
    rt_engine_ = std::make_shared<RTEngine>();
    this->lsi_ = std::make_shared<LSIRTNative<CONTEXT_T>>(ctx, rt_engine_);
    this->pip_ = std::make_shared<PIPRTNative<CONTEXT_T>>(ctx, rt_engine_);
  }

  void set_config(const QueryConfigRT& config) { config_ = config; }

  void Init() override {
    auto& ctx = this->ctx_;
    auto exec_root = ctx.get_exec_root();

    RTConfig rt_config = get_default_rt_config(exec_root);
    rt_engine_->Init(rt_config);

    size_t max_n_points = 0;
    size_t max_n_edges = 0;
    size_t total_n_edges = 0;

    FOR2 {
      auto map = ctx.get_map(im);
      CHECK(map != nullptr) << "MapOverlayNativeRT::Init: map " << im << " is null";

      const size_t np = map->get_points_num();
      const size_t ne = map->get_edges_num();

      this->closest_eids_[im].resize(np, static_cast<index_t>(DONTKNOW));
      this->point_in_polygon_[im].resize(np, static_cast<index_t>(DONTKNOW));

      eid_range_[im] = std::make_shared<thrust::device_vector<thrust::pair<size_t, size_t>>>();
      eid_range_[im]->clear();
      eid_range_[im]->reserve(ne);

      max_n_points = std::max(max_n_points, np);
      max_n_edges = std::max(max_n_edges, ne);
      total_n_edges += ne;

      traverse_handles_[im] = 0;
    }

    aabbs_.clear();
    aabbs_.reserve(max_n_edges);

    // Init LSI
    const size_t max_n_xsects = std::ceil(total_n_edges * config_.xsect_factor);
    this->lsi_->Init(max_n_xsects);
    // Init PIP
    this->pip_->Init(max_n_points);

    LOG(INFO) << "MapOverlayNativeRT::Init finished. "
              << "max_n_points=" << max_n_points << ", max_n_edges=" << max_n_edges << ", total_n_edges=" << total_n_edges;
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

  void IntersectEdge(int query_map_id) override {
    const int base_map_id = 1 - query_map_id;
    auto& stream = this->ctx_.get_stream();
    auto lsi = std::dynamic_pointer_cast<LSIRTNative<CONTEXT_T>>(this->lsi_);

    config_.eid_range = eid_range_[base_map_id];
    config_.handle = traverse_handles_[base_map_id];

    lsi->set_config(config_);
    lsi->Query(stream, query_map_id);
    stream.Sync();

    if (ShouldDumpStage(config_.dump_results, "lsi")) {
      DumpLSIResultsCSVEidOnly(rayjoin::DumpSubdir(config_.dump_dir, "results_lsi"), "native");
    }
  }

  void LocateVerticesInOtherMap(int query_map_id) override {
    auto& ctx = this->ctx_;
    Stream& stream = ctx.get_stream();
    auto pip = std::dynamic_pointer_cast<PIPRTNative<CONTEXT_T>>(this->pip_);
    auto base_map_id = 1 - query_map_id;
    auto d_base_map = ctx.get_map(base_map_id)->DeviceObject();
    auto d_query_map = ctx.get_map(query_map_id)->DeviceObject();
    auto d_points = d_query_map.get_points();

    config_.eid_range = eid_range_[base_map_id];
    config_.handle = traverse_handles_[base_map_id];

    pip->set_config(config_);
    pip->Query(stream, query_map_id, d_points);

    auto& closest_eid = this->pip_->get_closest_eids();

    thrust::copy(thrust::cuda::par.on(stream.cuda_stream()), closest_eid.begin(), closest_eid.end(), this->closest_eids_[query_map_id].begin());

    thrust::transform(thrust::cuda::par.on(stream.cuda_stream()),
                      closest_eid.begin(),
                      closest_eid.end(),
                      this->point_in_polygon_[query_map_id].begin(),
                      [=] __device__(index_t eid) {
                        if (eid == std::numeric_limits<index_t>::max()) {
                          return EXTERIOR_FACE_ID;
                        }
                        const auto& e = d_base_map.get_edge(eid);
                        return d_base_map.get_face_id(e);
                      });

    stream.Sync();

    if (rayjoin::ShouldDumpStage(config_.dump_results, "pip")) {
      DumpPIPResultsCSV(query_map_id, rayjoin::DumpSubdir(config_.dump_dir, "results_pip"), "native");
    }
  }

  void ComputeOutputPolygons() override {
    using point_t = typename CONTEXT_T::map_t::point_t;
    auto& ctx = this->ctx_;
    auto& stream = ctx.get_stream();
    auto pip = std::dynamic_pointer_cast<PIPRTNative<CONTEXT_T>>(this->pip_);
    auto xsects = this->lsi_->get_xsects();
    size_t n_xsects = xsects.size();

    FOR2 {
      thrust::device_vector<index_t> unique_eids;
      thrust::device_vector<uint32_t> n_xsects_per_edge;
      thrust::device_vector<uint32_t> xsect_index;
      thrust::device_vector<point_t> mid_points;

      auto& xsect_edges_sorted = xsect_edges_sorted_[im];

      xsect_edges_sorted.resize(n_xsects);
      thrust::copy(thrust::cuda::par.on(stream.cuda_stream()), xsects.data(), xsects.data() + n_xsects, xsect_edges_sorted.begin());

      thrust::sort(thrust::cuda::par.on(stream.cuda_stream()),
                   xsect_edges_sorted.begin(),
                   xsect_edges_sorted.end(),
                   [=] __device__(const xsect_t& xsect1, const xsect_t& xsect2) { return xsect1.eid[im] < xsect2.eid[im]; });

      ArrayView<xsect_t> d_xsect_edges_sorted(xsect_edges_sorted);
      auto query_map_id = im;
      auto base_map_id = 1 - im;
      auto d_query_map = ctx.get_map(query_map_id)->DeviceObject();
      auto d_base_map = ctx.get_map(base_map_id)->DeviceObject();

      unique_eids.resize(n_xsects);

      thrust::transform(thrust::cuda::par.on(stream.cuda_stream()),
                        xsect_edges_sorted.begin(),
                        xsect_edges_sorted.end(),
                        unique_eids.begin(),
                        [=] __device__(const xsect_t& xsect) { return xsect.eid[im]; });

      auto end = thrust::unique(thrust::cuda::par.on(stream.cuda_stream()), unique_eids.begin(), unique_eids.end());

      // =======================================================================
      // Get Starting and ending xsect index for each eid
      unique_eids.resize(end - unique_eids.begin());
      n_xsects_per_edge.resize(unique_eids.size());
      xsect_index.resize(unique_eids.size() + 1, 0);

      thrust::transform(
          thrust::cuda::par.on(stream.cuda_stream()), unique_eids.begin(), unique_eids.end(), n_xsects_per_edge.begin(), [=] __device__(index_t eid) {
            xsect_t dummy_xsect;
            dummy_xsect.eid[im] = eid;

            auto it = thrust::equal_range(thrust::seq,
                                          d_xsect_edges_sorted.begin(),
                                          d_xsect_edges_sorted.end(),
                                          dummy_xsect,
                                          [=] __device__(const xsect_t& xsect1, const xsect_t& xsect2) { return xsect1.eid[im] < xsect2.eid[im]; });

            return thrust::distance(it.first, it.second);
          });

      thrust::inclusive_scan(thrust::cuda::par.on(stream.cuda_stream()), n_xsects_per_edge.begin(), n_xsects_per_edge.end(), xsect_index.begin() + 1);
      stream.Sync();

      // =======================================================================
      // Compute Midpoints
      uint32_t n_mid_points = xsect_index[xsect_index.size() - 1] - unique_eids.size();
      mid_points.resize(n_mid_points);

      ArrayView<uint32_t> d_xsect_index(xsect_index);
      ArrayView<point_t> d_mid_points(mid_points);
      ArrayView<index_t> d_unique_eids(unique_eids);

      ForEach(stream, d_unique_eids.size(), [=] __device__(size_t idx) mutable {
        auto eid = d_unique_eids[idx];
        auto begin = d_xsect_index[idx];
        auto end = d_xsect_index[idx + 1];
        auto n_xsect = end - begin;

        if (n_xsect > 1) {
          const auto& e = d_query_map.get_edge(eid);
          const auto& p1 = d_query_map.get_point(e.p1_idx);
          auto* curr_xsects = d_xsect_edges_sorted.data() + begin;

          thrust::sort(thrust::seq, curr_xsects, d_xsect_edges_sorted.data() + end, [=](const xsect_t& xsect1, const xsect_t& xsect2) {
            auto d1 = SQ(xsect1.x - p1.x) + SQ(xsect1.y - p1.y);
            auto d2 = SQ(xsect2.x - p1.x) + SQ(xsect2.y - p1.y);

            if (d1 != d2) return d1 < d2;
            return xsect1.eid[1 - im] < xsect2.eid[1 - im];
          });

          for (int xsect_idx = 0; xsect_idx < n_xsect - 1; xsect_idx++) {
            xsect_t& xsect1 = *(curr_xsects + xsect_idx);
            xsect_t& xsect2 = *(curr_xsects + xsect_idx + 1);

            point_t mid_p;
            mid_p.x = (xsect1.x + xsect2.x) / coord_t(2);
            mid_p.y = (xsect1.y + xsect2.y) / coord_t(2);

            assert(begin + xsect_idx - idx < d_mid_points.size());
            d_mid_points[begin + xsect_idx - idx] = mid_p;
          }
        }
      });

      stream.Sync();

      // =======================================================================
      // PIP each midpoints
      config_.eid_range = eid_range_[base_map_id];
      config_.handle = traverse_handles_[base_map_id];

      pip->set_config(config_);
      pip->Query(stream, query_map_id, d_mid_points);
      stream.Sync();

      ArrayView<index_t> d_mid_point_closest_eid(pip->get_closest_eids());

      ForEach(stream, d_unique_eids.size(), [=] __device__(size_t idx) mutable {
        auto begin = d_xsect_index[idx];
        auto end = d_xsect_index[idx + 1];
        auto n_xsect = end - begin;

        if (n_xsect > 1) {
          auto* curr_xsects = d_xsect_edges_sorted.data() + begin;

          for (int xsect_idx = 0; xsect_idx < n_xsect - 1; xsect_idx++) {
            xsect_t& xsect1 = *(curr_xsects + xsect_idx);
            auto eid = d_mid_point_closest_eid[begin + xsect_idx - idx];
            polygon_id_t ipol = EXTERIOR_FACE_ID;

            if (eid != std::numeric_limits<index_t>::max()) {
              const auto& e = d_base_map.get_edge(eid);
              ipol = d_base_map.get_face_id(e);
            }

            xsect1.mid_point_polygon_id = ipol;
          }
        }
      });

      stream.Sync();
    }

    if (ShouldDumpStage(config_.dump_results, "pipmid")) {
      const auto out_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_mid");
      DumpComputeOutputPolygonsCSV(0, out_dir, "native");
      DumpComputeOutputPolygonsCSV(1, out_dir, "native");
    }
  }

  void WriteResult(const std::string& path) override { WriteOutputChainNative(this->ctx_, xsect_edges_sorted_, this->point_in_polygon_, path); }

 private:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;

  OptixTraversableHandle traverse_handles_[2]{};
  thrust::device_vector<OptixAabb> aabbs_;
  std::shared_ptr<thrust::device_vector<thrust::pair<size_t, size_t>>> eid_range_[2];

  // ComputeOutputPolygons
  thrust::device_vector<xsect_t> xsect_edges_sorted_[2];

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

  void DumpLSIResultsCSVEidOnly(const std::string& out_dir, const std::string& impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    thrust::host_vector<xsect_t> h_xsects;
    this->lsi_->CopyTo(h_xsects);

    const std::string path = out_dir + "/" + impl_tag + "_lsi.csv";

    std::vector<std::pair<uint64_t, uint64_t>> pairs;
    pairs.reserve(h_xsects.size());

    for (const auto& x: h_xsects) {
      uint64_t eid1 = static_cast<uint64_t>(x.eid[0]);
      uint64_t eid2 = static_cast<uint64_t>(x.eid[1]);

      if (eid1 > eid2) {
        std::swap(eid1, eid2);
      }

      pairs.emplace_back(eid1, eid2);
    }

    std::sort(pairs.begin(), pairs.end());

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpLSIResultsCSVEidOnly: failed to open " << path;
      return;
    }

    ofs << "eid1,eid2\n";
    for (const auto& [eid1, eid2]: pairs) {
      ofs << eid1 << "," << eid2 << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpLSIResultsCSVEidOnly: wrote " << path;
  }

  void DumpLSIResultsCSV(const std::string& out_dir, const std::string& impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    thrust::host_vector<xsect_t> h_xsects;
    this->lsi_->CopyTo(h_xsects);

    struct Row {
      index_t eid0;
      index_t eid1;
      coord_t x;
      coord_t y;
      int mid_point_polygon_id;
    };

    std::vector<Row> rows;
    rows.reserve(h_xsects.size());

    for (const auto& xsect: h_xsects) {
      rows.push_back(Row{
          .eid0 = static_cast<index_t>(xsect.eid[0]),
          .eid1 = static_cast<index_t>(xsect.eid[1]),
          .x = xsect.x,
          .y = xsect.y,
          .mid_point_polygon_id = xsect.mid_point_polygon_id,
      });
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
      if (a.eid0 != b.eid0) return a.eid0 < b.eid0;
      if (a.eid1 != b.eid1) return a.eid1 < b.eid1;
      if (a.x != b.x) return a.x < b.x;
      if (a.y != b.y) return a.y < b.y;
      return a.mid_point_polygon_id < b.mid_point_polygon_id;
    });

    const std::string path = out_dir + "/" + impl_tag + "_lsi.csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to open " << path;
      return;
    }

    ofs << std::setprecision(17);
    ofs << "eid0,eid1,x,y,mid_point_polygon_id\n";

    for (const auto& r: rows) {
      ofs << r.eid0 << "," << r.eid1 << "," << static_cast<double>(r.x) << "," << static_cast<double>(r.y) << "," << r.mid_point_polygon_id << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpLSIResultsCSV: wrote " << path;
  }


  void DumpPIPResultsCSV(int query_map_id, const std::string& out_dir, const std::string& impl_tag) const {
    namespace fs = std::filesystem;

    fs::create_directories(out_dir);

    const auto& d_closest = this->closest_eids_[query_map_id];
    const auto& d_poly = this->point_in_polygon_[query_map_id];

    thrust::host_vector<index_t> h_closest = d_closest;
    thrust::host_vector<index_t> h_poly = d_poly;

    if (h_closest.size() != h_poly.size()) {
      LOG(ERROR) << "DumpPIPResultsCSV: size mismatch for query_map_id=" << query_map_id << " closest=" << h_closest.size()
                 << " poly=" << h_poly.size();
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

    for (size_t point_id = 0; point_id < h_closest.size(); ++point_id) {
      const index_t eid = h_closest[point_id];
      const index_t pid = h_poly[point_id];

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

  void DumpComputeOutputPolygonsCSV(int query_map_id, const std::string& out_dir, const std::string& impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    if (query_map_id < 0 || query_map_id > 1) {
      LOG(ERROR) << "DumpComputeOutputPolygonsCSV: invalid query_map_id=" << query_map_id;
      return;
    }

    const auto& d_xsects = xsect_edges_sorted_[query_map_id];
    thrust::host_vector<xsect_t> h_xsects = d_xsects;

    const std::string path = out_dir + "/" + impl_tag + "_pipmid_map_" + std::to_string(query_map_id) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpComputeOutputPolygonsCSV: failed to open " << path;
      return;
    }

    ofs << "query_map_id,eid_self,eid_other,mid_point_polygon_id\n";

    for (const auto& x: h_xsects) {
      ofs << query_map_id << "," << static_cast<index_t>(x.eid[query_map_id]) << "," << static_cast<index_t>(x.eid[1 - query_map_id]) << ","
          << static_cast<index_t>(x.mid_point_polygon_id) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpComputeOutputPolygonsCSV: wrote " << path;
  }
};

}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_NATIVE_RT_H
