#ifndef RAYJOIN_MAP_H
#define RAYJOIN_MAP_H

#include "../engine/vk_buffer_readback.h"
#include "edge_init_pass_i64_raii.h"
#include "glog/logging.h"
#include "planar_graph.h"
#include "scale_points_raii.h"
#include "vk/map/gpu_edge_types.h"
#include "vk/map/scaling.h"

// For Dumping
#include <filesystem>
#include <fstream>

namespace rayjoin {
namespace vk {

template<typename COEFFICIENT_T>
struct alignas(16) Edge {
  COEFFICIENT_T a;
  COEFFICIENT_T b;
  COEFFICIENT_T c;

  index_t eid;
  index_t p1_idx;
  index_t p2_idx;
  index_t left_polygon_id;
  index_t right_polygon_id;

  uint32_t pad0;
  uint32_t pad1;
  uint32_t pad2;
};

static_assert(sizeof(__int128) == 16);
static_assert(alignof(__int128) == 16);
static_assert(sizeof(Edge<__int128>) == 80);
static_assert(alignof(Edge<__int128>) == 16);
static_assert(std::is_trivially_copyable_v<Edge<__int128>>);

template<typename INTERNAL_COORD_T, typename EDGE_COEFFICIENT_T>
class Map {
 public:
  using internal_coord_t = INTERNAL_COORD_T;
  using coefficient_t = EDGE_COEFFICIENT_T;
  using point_t = Vec2<internal_coord_t>;
  using edge_t = Edge<coefficient_t>;

  Map() = delete;
  Map(int id) : id_(id) {}

  ~Map() = default;

  template<typename SRC_COORD_T>
  void Init(const Scaling<SRC_COORD_T, INTERNAL_COORD_T>& scaling, const PlanarGraph<SRC_COORD_T>& pgraph) {
    LOG(INFO) << "Init Map-" << id_ << " From PGraphs";

    /* ------------------------------------------------------------ */
    /* Step1: scale points                                          */
    /* ------------------------------------------------------------ */
    struct ScalePointsParams {
      uint32_t count;
      uint32_t pad0;
      uint32_t pad1;
      uint32_t pad2;
    };

    static_assert(std::is_trivially_copyable_v<ScalePointsParams>);
    static_assert(sizeof(ScalePointsParams) == 16);

    point_count_ = static_cast<uint32_t>(pgraph.points.size());

    srcPointsDev_.Init(sizeof(SrcPointD) * point_count_);
    scaledPointsDev_.Init(sizeof(DstPointI64) * point_count_);
    scalingDev_.Init(sizeof(Scaling<SRC_COORD_T, INTERNAL_COORD_T>));

    writeToStorageBuffer(srcPointsDev_, pgraph.points);
    writeToStorageBuffer(scalingDev_, scaling);

    std::string spvPathScaling = std::string(SHADER_DIR) + "/scale_points_d2_i64.spv";

    ScalePointsParams scale_params{};
    scale_params.count = point_count_;

    RunComputePass(point_count_, spvPathScaling.c_str(), scale_params, srcPointsDev_, scaledPointsDev_, scalingDev_);

    DebugPrintScaledPoints(scaling, pgraph, point_count_);

    /* ------------------------------------------------------------ */
    /* Step2: initialize edges                                      */
    /* ------------------------------------------------------------ */
    struct EdgeInitParams {
      uint32_t num_points;
      uint32_t num_chains;
      uint32_t num_edges;
      uint32_t pad0;
    };

    static_assert(std::is_trivially_copyable_v<EdgeInitParams>);
    static_assert(sizeof(EdgeInitParams) == 16);

    chain_count_ = static_cast<uint32_t>(pgraph.chains.size());
    edge_count_ = point_count_ - chain_count_;

    chainsDev_.Init(sizeof(Chain) * chain_count_);
    rowDev_.Init(sizeof(index_t) * (chain_count_ + 1));
    edgesDev_.Init(sizeof(edge_t) * edge_count_);

    writeToStorageBuffer(chainsDev_, pgraph.chains);
    writeToStorageBuffer(rowDev_, pgraph.row_index);

    std::string spvPathEdge = std::string(SHADER_DIR) + "/edge_init_i128.spv";

    EdgeInitParams edge_params{};
    edge_params.num_points = point_count_;
    edge_params.num_chains = chain_count_;
    edge_params.num_edges = edge_count_;

    RunComputePass(point_count_, spvPathEdge.c_str(), edge_params, scaledPointsDev_, chainsDev_, rowDev_, edgesDev_);

    LOG(INFO) << "Map-" << id_ << ": initialized " << edge_count_ << " edges on GPU";

    DebugPrintEdges(point_count_);
  }

  size_t get_edges_num() const { return edge_count_; }
  size_t get_points_num() const { return point_count_; }

  const VkDeviceBuf& getPointsBuffer() const { return scaledPointsDev_; }
  const VkDeviceBuf& getEdgesBuffer() const { return edgesDev_; }
  const VkDeviceBuf& getScalingBuffer() const { return scalingDev_; }

 private:
  int id_;

  uint32_t point_count_ = 0;
  uint32_t chain_count_ = 0;
  uint32_t edge_count_ = 0;

  VkDeviceBuf srcPointsDev_{};
  VkDeviceBuf scaledPointsDev_{};
  VkDeviceBuf scalingDev_{};

  VkDeviceBuf chainsDev_{};
  VkDeviceBuf rowDev_{};
  VkDeviceBuf edgesDev_{};

  static std::string Int128ToString(__int128 v) {
    if (v == 0) return "0";

    bool neg = v < 0;
    unsigned __int128 x = neg ? static_cast<unsigned __int128>(-v) : static_cast<unsigned __int128>(v);

    std::string s;
    while (x > 0) {
      s.push_back(static_cast<char>('0' + (x % 10)));
      x /= 10;
    }
    if (neg) s.push_back('-');
    std::reverse(s.begin(), s.end());
    return s;
  }

  template<typename SRC_COORD_T>
  void DebugPrintScaledPoints(const Scaling<SRC_COORD_T, INTERNAL_COORD_T>& scaling,
                              const PlanarGraph<SRC_COORD_T>& pgraph,
                              uint32_t point_count) const {
    uint32_t checkCount = std::min<uint32_t>(point_count, 10);

    auto gpuPts = readBackStorageBuffer<DstPointI64>(scaledPointsDev_, checkCount);

    LOG(INFO) << "Map-" << id_ << " GPU readback (first " << checkCount << " points):";

    for (uint32_t i = 0; i < checkCount; ++i) {
      auto& srcp = pgraph.points[i];

      int64_t gpu_x = gpuPts[i].x;
      int64_t gpu_y = gpuPts[i].y;

      int64_t cpu_x = scaling.ScaleX(srcp.x);
      int64_t cpu_y = scaling.ScaleY(srcp.y);

      double unscaled_x = scaling.UnscaleX(gpu_x);
      double unscaled_y = scaling.UnscaleY(gpu_y);

      LOG(INFO) << "i=" << i << " src=(" << srcp.x << "," << srcp.y << ")"
                << " gpu=(" << gpu_x << "," << gpu_y << ")"
                << " cpu=(" << cpu_x << "," << cpu_y << ")"
                << " unscaled=(" << unscaled_x << "," << unscaled_y << ")";
    }
  }

  // void DebugPrintEdges(uint32_t point_count) const {
  //   uint32_t checkEdges = std::min<uint32_t>(edge_count_, 10);
  //
  //   auto gpuEdges = readBackStorageBuffer<edge_t>(edgesDev_, checkEdges);
  //   auto gpuPts = readBackStorageBuffer<DstPointI64>(scaledPointsDev_, point_count);
  //
  //   LOG(INFO) << "Map-" << id_ << " GPU edge readback (first " << checkEdges << " edges):";
  //
  //   for (uint32_t i = 0; i < checkEdges; ++i) {
  //     const auto& e = gpuEdges[i];
  //
  //     const auto& p1 = gpuPts[e.p1_idx];
  //     const auto& p2 = gpuPts[e.p2_idx];
  //
  //     __int128 a = static_cast<__int128>(p1.y) - static_cast<__int128>(p2.y);
  //     __int128 b = static_cast<__int128>(p2.x) - static_cast<__int128>(p1.x);
  //     __int128 c = -(static_cast<__int128>(p1.x) * a) - (static_cast<__int128>(p1.y) * b);
  //
  //     if (b < 0) {
  //       a = -a;
  //       b = -b;
  //       c = -c;
  //     }
  //
  //     bool ok = (a == e.a) && (b == e.b) && (c == e.c);
  //
  //     LOG(INFO) << "eid=" << e.eid << " p1=" << e.p1_idx << " p2=" << e.p2_idx << " GPU=(" << Int128ToString(e.a) << "," << Int128ToString(e.b) <<
  //     ","
  //               << Int128ToString(e.c) << ")"
  //               << " CPU=(" << Int128ToString(a) << "," << Int128ToString(b) << "," << Int128ToString(c) << ")"
  //               << " match=" << (ok ? "YES" : "NO");
  //   }
  // }

  void DebugPrintEdges(uint32_t point_count) const {
    uint32_t checkEdges = std::min<uint32_t>(edge_count_, 10);

    auto gpuEdges = readBackStorageBuffer<edge_t>(edgesDev_, checkEdges);
    auto gpuPts = readBackStorageBuffer<DstPointI64>(scaledPointsDev_, point_count);

    LOG(INFO) << "Map-" << id_ << " GPU edge readback (first " << checkEdges << " edges):";

    for (uint32_t i = 0; i < checkEdges; ++i) {
      const auto& e = gpuEdges[i];

      if (e.p1_idx >= point_count || e.p2_idx >= point_count) {
        LOG(ERROR) << "eid=" << e.eid << " invalid p1=" << e.p1_idx << " p2=" << e.p2_idx << " point_count=" << point_count;
        continue;
      }

      const auto& p1 = gpuPts[e.p1_idx];
      const auto& p2 = gpuPts[e.p2_idx];

      __int128 a = static_cast<__int128>(p1.y) - static_cast<__int128>(p2.y);
      __int128 b = static_cast<__int128>(p2.x) - static_cast<__int128>(p1.x);
      __int128 c = -(static_cast<__int128>(p1.x) * a) - (static_cast<__int128>(p1.y) * b);

      if (b < 0) {
        a = -a;
        b = -b;
        c = -c;
      }

      bool ok = (a == e.a) && (b == e.b) && (c == e.c);

      LOG(INFO) << "eid=" << e.eid << " p1=" << e.p1_idx << " p2=" << e.p2_idx << " GPU=(" << Int128ToString(e.a) << "," << Int128ToString(e.b) << ","
                << Int128ToString(e.c) << ")"
                << " CPU=(" << Int128ToString(a) << "," << Int128ToString(b) << "," << Int128ToString(c) << ")"
                << " match=" << (ok ? "YES" : "NO");
    }
  }

 public:
  void DumpScalingPointsCSV(const std::string& out_dir, const std::string& impl_tag) const {
    namespace fs = std::filesystem;

    fs::create_directories(out_dir);

    const uint32_t n_points = point_count_;
    auto gpuPts = readBackStorageBuffer<DstPointI64>(scaledPointsDev_, n_points);

    if (gpuPts.size() != n_points) {
      LOG(ERROR) << "DumpScalingPointsCSV: failed to read scaled points for map=" << id_ << " gpuPts.size=" << gpuPts.size()
                 << " expected=" << n_points;
      return;
    }

    const std::string path = out_dir + "/" + impl_tag + "_scaling_map_" + std::to_string(id_) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpScalingPointsCSV: failed to open " << path;
      return;
    }

    ofs << "map_id,point_id,x,y\n";

    for (uint32_t point_id = 0; point_id < n_points; ++point_id) {
      ofs << id_ << "," << point_id << "," << static_cast<long long>(gpuPts[point_id].x) << "," << static_cast<long long>(gpuPts[point_id].y) << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpScalingPointsCSV: wrote " << path;
  }
};

}  // namespace vk
}  // namespace rayjoin

#endif
