#ifndef RAYJOIN_MAP_NS_H
#define RAYJOIN_MAP_NS_H

#include <filesystem>
#include <thrust/device_vector.h>

#include "map/bounding_box.h"
#include "map/planar_graph.h"
#include "map/scaling.h"
#include "util/array_view.h"
#include "util/derived_atomic_functions.h"
#include "util/launcher.h"
#include "util/shared_value.h"
#include "util/stream.h"
#include "util/type_traits.h"

namespace rayjoin {
namespace dev {

template<typename COORD_T>
struct EdgeEquationNative {
  COORD_T a, b, c;  // ax + by + c = 0

  EdgeEquationNative() = default;

  template<typename POINT_T>
  DEV_HOST EdgeEquationNative(const POINT_T& p1, const POINT_T& p2) {
    a = p1.y - p2.y;
    b = p2.x - p1.x;
    c = -(p1.x * a + p1.y * b);

    assert(a != COORD_T(0) || b != COORD_T(0));

    // Keep canonical orientation if you still want deterministic comparison.
    if (b < COORD_T(0) || (b == COORD_T(0) && a < COORD_T(0))) {
      a = -a;
      b = -b;
      c = -c;
    }
  }
};

template<typename COORD_T>
struct __builtin_align__(16) EdgeNative : public EdgeEquationNative<COORD_T> {
  index_t eid;
  index_t p1_idx, p2_idx;
  index_t left_polygon_id, right_polygon_id;
};

template<typename COORD_T>
class MapNativeDev {
 public:
  using coord_t = COORD_T;
  using point_t = typename cuda_vec<coord_t>::type_2d;
  using edge_t = EdgeNative<coord_t>;

  MapNativeDev() = default;
  DEV_HOST MapNativeDev(char id, ArrayView<point_t> points, ArrayView<edge_t> edges) : id_(id), points_(points), edges_(edges) {}

  DEV_HOST_INLINE int get_id() const { return id_; }
  DEV_HOST_INLINE size_t get_points_num() const { return points_.size(); }
  DEV_HOST_INLINE size_t get_edges_num() const { return edges_.size(); }

  DEV_INLINE const point_t& get_point(size_t point_idx) const { return points_[point_idx]; }
  DEV_INLINE const edge_t& get_edge(size_t edge_idx) const { return edges_[edge_idx]; }

  DEV_HOST_INLINE ArrayView<point_t> get_points() const { return points_; }
  DEV_HOST_INLINE ArrayView<edge_t> get_edges() const { return edges_; }

  DEV_HOST_INLINE polygon_id_t get_face_id(const edge_t& e) const {
    return (get_point(e.p1_idx).x < get_point(e.p2_idx).x) ? e.right_polygon_id : e.left_polygon_id;
  }

 private:
  char id_;
  ArrayView<point_t> points_;
  ArrayView<edge_t> edges_;
};

}  // namespace dev

template<typename COORD_T>
class MapNative {
 public:
  using coord_t = COORD_T;
  using point_t = typename cuda_vec<coord_t>::type_2d;
  using edge_t = dev::EdgeNative<coord_t>;
  using dev_map_t = dev::MapNativeDev<coord_t>;

  explicit MapNative(int id) : id_(id) {}

  void LoadFrom(Stream& stream, const PlanarGraph<coord_t>& pgraph) {
    points_ = pgraph.points;
    edges_.resize(pgraph.points.size() - pgraph.chains.size());

    ArrayView<Chain> v_chains(pgraph.chains);
    ArrayView<index_t> v_row_index(pgraph.row_index);

    LaunchKernel(
        stream,
        [=] __device__(ArrayView<point_t> points, ArrayView<edge_t> edges) {
          auto warp_id = TID_1D / 32;
          auto n_warps = TOTAL_THREADS_1D / 32;
          auto lane_id = threadIdx.x % 32;

          for (size_t ichain = warp_id; ichain < v_chains.size(); ichain += n_warps) {
            const auto& chain = v_chains[ichain];

            for (auto p_idx = v_row_index[ichain] + lane_id; p_idx < v_row_index[ichain + 1] - 1; p_idx += 32) {
              auto eid = p_idx - ichain;
              auto& e = edges[eid];

              e.eid = eid;
              e.p1_idx = p_idx;
              e.p2_idx = p_idx + 1;
              e.left_polygon_id = chain.left_polygon_id;
              e.right_polygon_id = chain.right_polygon_id;

              const auto& p1 = points[e.p1_idx];
              const auto& p2 = points[e.p2_idx];

              e.a = p1.y - p2.y;
              e.b = p2.x - p1.x;
              e.c = -(p1.x * e.a + p1.y * e.b);

              assert(e.a != coord_t(0) || e.b != coord_t(0));

              if (e.b < coord_t(0) || (e.b == coord_t(0) && e.a < coord_t(0))) {
                e.a = -e.a;
                e.b = -e.b;
                e.c = -e.c;
              }
            }
          }
        },
        ArrayView<point_t>(points_),
        ArrayView<edge_t>(edges_));

    stream.Sync();
  }

  dev_map_t DeviceObject() const { return dev_map_t(id_, ArrayView<point_t>(points_), ArrayView<edge_t>(edges_)); }

  void D2H() {
    if (h_points_.empty()) {
      h_points_ = points_;
    }
    if (h_edges_.empty()) {
      h_edges_ = edges_;
    }
  }

  char get_id() const { return id_; }
  size_t get_points_num() const { return points_.size(); }
  size_t get_edges_num() const { return edges_.size(); }

  const point_t& get_point(size_t point_idx) const {
    assert(point_idx < h_points_.size());
    return h_points_[point_idx];
  }

  const edge_t& get_edge(size_t edge_idx) const {
    assert(edge_idx < h_edges_.size());
    return h_edges_[edge_idx];
  }

  std::string EndpointsToString(size_t eid) const { return EndpointsToString(get_edge(eid)); }

  std::string EndpointsToString(const edge_t& e) const {
    auto p1 = get_point(e.p1_idx);
    auto p2 = get_point(e.p2_idx);

    std::string s;
    s.resize(1024);
    auto n = snprintf(const_cast<char*>(s.c_str()),
                      s.size(),
                      "(%.17g, %.17g) - (%.17g, %.17g)",
                      static_cast<double>(p1.x),
                      static_cast<double>(p1.y),
                      static_cast<double>(p2.x),
                      static_cast<double>(p2.y));
    s.resize(n);
    return s;
  }

  void DumpPointsCSV(const std::string& out_dir, const std::string& impl_tag) {
    namespace fs = std::filesystem;

    fs::create_directories(out_dir);

    D2H();

    const std::string path = out_dir + "/" + impl_tag + "_points_map_" + std::to_string(id_) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpPointsCSV: failed to open " << path;
      return;
    }

    ofs << "map_id,point_id,x,y\n";
    ofs << std::setprecision(17);

    for (size_t point_id = 0; point_id < h_points_.size(); ++point_id) {
      const auto& p = h_points_[point_id];
      ofs << id_ << "," << point_id << "," << static_cast<double>(p.x) << "," << static_cast<double>(p.y) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpPointsCSV: wrote " << path;
  }

  void DumpEdgesCSV(const std::string& out_dir, const std::string& impl_tag) {
    namespace fs = std::filesystem;

    fs::create_directories(out_dir);

    D2H();

    const std::string path = out_dir + "/" + impl_tag + "_edges_map_" + std::to_string(id_) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpEdgesCSV: failed to open " << path;
      return;
    }

    ofs << "map_id,eid,p1_idx,p2_idx,left_polygon_id,right_polygon_id,a,b,c\n";
    ofs << std::setprecision(17);

    for (size_t edge_id = 0; edge_id < h_edges_.size(); ++edge_id) {
      const auto& e = h_edges_[edge_id];
      ofs << id_ << "," << e.eid << "," << e.p1_idx << "," << e.p2_idx << "," << e.left_polygon_id << "," << e.right_polygon_id << ","
          << static_cast<double>(e.a) << "," << static_cast<double>(e.b) << "," << static_cast<double>(e.c) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpEdgesCSV: wrote " << path;
  }

 private:
  int id_;
  thrust::device_vector<point_t> points_;
  thrust::device_vector<edge_t> edges_;
  pinned_vector<point_t> h_points_;
  pinned_vector<edge_t> h_edges_;
};

}  // namespace rayjoin

#endif  // RAYJOIN_MAP_NS_H
