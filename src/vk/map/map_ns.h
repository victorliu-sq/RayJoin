#ifndef RAYJOIN_MAP_NS_H
#define RAYJOIN_MAP_NS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include "../engine/vk_buffer_readback.h"
#include "glog/logging.h"
#include "planar_graph.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/edge_init_pass_raii_ns.h"

namespace rayjoin {
namespace vk {

template<EdgeCoeffType COEFF_T>
struct Edge {
  COEFF_T a;
  COEFF_T b;
  COEFF_T c;

  uint64_t eid;
  uint64_t p1_idx;
  uint64_t p2_idx;
  uint64_t left_polygon_id;
  uint64_t right_polygon_id;
};

template<PointCoordType POINT_COORD_T>
class MapNS {
 public:
  using coord_t = POINT_COORD_T;
  using coeff_t = POINT_COORD_T;
  using point_t = Vec2<coord_t>;
  using edge_t = Edge<coord_t>;

  MapNS() = delete;
  explicit MapNS(int id) : id_(id) {}

  ~MapNS() = default;

  void LoadFrom(const PlanarGraph<coord_t>& pgraph) {
    LOG(INFO) << "Init Map-" << id_ << " From PGraph (no scaling)";

    point_count_ = static_cast<uint32_t>(pgraph.points.size());
    chain_count_ = static_cast<uint32_t>(pgraph.chains.size());
    edge_count_ = point_count_ - chain_count_;

    pointsDev_.Init(sizeof(point_t) * point_count_);
    chainsDev_.Init(sizeof(Chain) * chain_count_);
    rowDev_.Init(sizeof(index_t) * (chain_count_ + 1));
    edgesDev_.Init(sizeof(edge_t) * edge_count_);

    writeToStorageBuffer(pointsDev_, pgraph.points);
    writeToStorageBuffer(chainsDev_, pgraph.chains);
    writeToStorageBuffer(rowDev_, pgraph.row_index);

    edge_pass_ = std::make_unique<EdgeInitPassRAIINS>(pointsDev_, chainsDev_, rowDev_, edgesDev_, point_count_, chain_count_);

    edge_pass_->run();

    LOG(INFO) << "Map-" << id_ << ": initialized " << edge_count_ << " edges on GPU (no scaling)";

    // DebugPrintEdges();
  }

  size_t get_edges_num() const { return edge_count_; }
  size_t get_points_num() const { return point_count_; }

  const VkDeviceBuf& getPointsBuffer() const { return pointsDev_; }
  const VkDeviceBuf& getEdgesBuffer() const { return edgesDev_; }

 private:
  static bool NearlyEqual(double x, double y, double eps = 1e-9) {
    double diff = std::abs(x - y);
    double scale = std::max({1.0, std::abs(x), std::abs(y)});
    return diff <= eps * scale;
  }

  void DebugPrintEdges() const {
    uint32_t checkEdges = std::min<uint32_t>(edge_count_, 10);

    auto gpuEdges = readBackStorageBuffer<edge_t>(edgesDev_, checkEdges);
    auto gpuPts = readBackStorageBuffer<point_t>(pointsDev_, point_count_);

    LOG(INFO) << "Map-" << id_ << " GPU edge readback (first " << checkEdges << " edges):";

    for (uint32_t i = 0; i < checkEdges; ++i) {
      const auto& e = gpuEdges[i];

      const auto& p1 = gpuPts[e.p1_idx];
      const auto& p2 = gpuPts[e.p2_idx];

      coeff_t a = static_cast<coeff_t>(p1.y - p2.y);
      coeff_t b = static_cast<coeff_t>(p2.x - p1.x);
      coeff_t c = static_cast<coeff_t>(-(p1.x * a) - (p1.y * b));

      if (b < static_cast<coeff_t>(0)) {
        a = -a;
        b = -b;
        c = -c;
      }

      bool ok;
      if constexpr (std::is_floating_point_v<coeff_t>) {
        ok = NearlyEqual(static_cast<double>(a), static_cast<double>(e.a)) && NearlyEqual(static_cast<double>(b), static_cast<double>(e.b)) &&
             NearlyEqual(static_cast<double>(c), static_cast<double>(e.c));
      } else {
        ok = (a == e.a) && (b == e.b) && (c == e.c);
      }

      LOG(INFO) << "eid=" << e.eid << " p1=" << e.p1_idx << " p2=" << e.p2_idx << " GPU=(" << e.a << "," << e.b << "," << e.c << ")"
                << " CPU=(" << a << "," << b << "," << c << ")"
                << " match=" << (ok ? "YES" : "NO");
    }
  }

 private:
  int id_;

  std::unique_ptr<EdgeInitPassRAIINS> edge_pass_;

  uint32_t point_count_ = 0;
  uint32_t chain_count_ = 0;
  uint32_t edge_count_ = 0;

  VkDeviceBuf pointsDev_{};
  VkDeviceBuf chainsDev_{};
  VkDeviceBuf rowDev_{};
  VkDeviceBuf edgesDev_{};
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_NS_H
