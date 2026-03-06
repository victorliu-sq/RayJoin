#ifndef RAYJOIN_MAP_H
#define RAYJOIN_MAP_H

#include "edge_init_pass_i64_raii.h"
#include "glog/logging.h"
#include "planar_graph.h"
#include "scale_points_raii.h"
#include "vk/map/gpu_edge_types.h"
#include "vk/map/scaling.h"
#include "vk/map/vk_debug_readback.h"

namespace rayjoin {
namespace vk {

struct Edge {
  int64_t a;
  int64_t b;
  int64_t c;

  uint64_t eid;
  uint64_t p1_idx;
  uint64_t p2_idx;
  uint64_t left_polygon_id;
  uint64_t right_polygon_id;
};

template <typename INTERNAL_COORD_T, typename COEFFICIENT_T>
class Map {
 public:
  using internal_coord_t = INTERNAL_COORD_T;
  using coefficient_t = COEFFICIENT_T;
  using point_t = Vec2<internal_coord_t>;

  Map() = delete;
  Map(int id) : id_(id) {}

  ~Map() {
    auto& vk_ctx = GetVkComputeContext();
    // Scaling
    vmaDestroyBufferSafe(vk_ctx.vma, srcPointsDev_);
    vmaDestroyBufferSafe(vk_ctx.vma, scaledPointsDev_);
    // Edge Init
    vmaDestroyBufferSafe(vk_ctx.vma, chainsDev_);
    vmaDestroyBufferSafe(vk_ctx.vma, rowDev_);
    vmaDestroyBufferSafe(vk_ctx.vma, edgesDev_);
  }

  template <typename SRC_COORD_T>
  void LoadFrom(const Scaling<SRC_COORD_T, INTERNAL_COORD_T>& scaling,
                const PlanarGraph<SRC_COORD_T>& pgraph) {
    LOG(INFO) << "Init Map-" << id_ << " From PGraphs";
    auto& vk_ctx = GetVkComputeContext();

    /* ------------------------------------------------------------ */
    /* Step1: scale points                                          */
    /* ------------------------------------------------------------ */
    uint32_t point_count = static_cast<uint32_t>(pgraph.points.size());
    /* allocate GPU buffers */
    srcPointsDev_ = createStorageBuffer<SrcPointD>(vk_ctx.vma, point_count);
    scaledPointsDev_ =
        createStorageBuffer<DstPointI64>(vk_ctx.vma, point_count);
    /* upload CPU → GPU */
    writeToBuffer(srcPointsDev_, pgraph.points);
    /* run scaling compute pass */
    std::string spvPathScaling =
        std::string(SHADER_DIR) + "/scale_points_d2_i64.spv";
    scale_pass_ = std::make_unique<ScalePointsPassD2I64RAII>(
        spvPathScaling.c_str(), srcPointsDev_, scaledPointsDev_, point_count,
        scaling.rx(), scaling.ry(), scaling.deltax(), scaling.deltay());

    scale_pass_->run();
    DebugPrintScaledPoints(scaling, pgraph, point_count);

    /* ------------------------------------------------------------ */
    /* Step2: initialize edges                                      */
    /* ------------------------------------------------------------ */
    /* allocate GPU buffers */
    chain_count_ = static_cast<uint32_t>(pgraph.chains.size());
    edge_count_ = point_count - chain_count_;

    chainsDev_ = createStorageBuffer<Chain>(vk_ctx.vma, chain_count_);
    rowDev_ = createStorageBuffer<index_t>(vk_ctx.vma, chain_count_ + 1);
    edgesDev_ = createStorageBuffer<Edge>(vk_ctx.vma, edge_count_);

    /* upload CPU → GPU */
    writeToBuffer(chainsDev_, pgraph.chains);
    writeToBuffer(rowDev_, pgraph.row_index);

    /* run edge init compute pass */
    std::string spvPath = std::string(SHADER_DIR) + "/edge_init_i64.spv";
    edge_pass_ = std::make_unique<EdgeInitPassI64RAII>(
        spvPath.c_str(), scaledPointsDev_, chainsDev_, rowDev_, edgesDev_,
        point_count, chain_count_);

    edge_pass_->run();
    LOG(INFO) << "Map-" << id_ << ": initialized " << edge_count_
              << " edges on GPU";

    DebugPrintEdges(point_count);
  }

  size_t get_edges_num() const { return edge_count_; }

 private:
  int id_;

  std::unique_ptr<ScalePointsPassD2I64RAII> scale_pass_;
  std::unique_ptr<EdgeInitPassI64RAII> edge_pass_;

  uint32_t chain_count_ = 0;
  uint32_t edge_count_ = 0;

  /* GPU buffers owned by Map */

  AllocBuf srcPointsDev_{};
  AllocBuf scaledPointsDev_{};
  AllocBuf chainsDev_{};
  AllocBuf rowDev_{};
  AllocBuf edgesDev_{};

  /* ------------------------------------------------------------ */
  /* Debug helpers                                                 */
  /* ------------------------------------------------------------ */

  template <typename SRC_COORD_T>
  void DebugPrintScaledPoints(
      const Scaling<SRC_COORD_T, INTERNAL_COORD_T>& scaling,
      const PlanarGraph<SRC_COORD_T>& pgraph, uint32_t point_count) const {
    uint32_t checkCount = std::min<uint32_t>(point_count, 10);

    auto gpuPts = readBackBuffer<DstPointI64>(scaledPointsDev_, checkCount);

    LOG(INFO) << "Map-" << id_ << " GPU readback (first " << checkCount
              << " points):";

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

  void DebugPrintEdges(uint32_t point_count) const {
    uint32_t checkEdges = std::min<uint32_t>(edge_count_, 10);

    auto gpuEdges = readBackBuffer<Edge>(edgesDev_, checkEdges);
    auto gpuPts = readBackBuffer<DstPointI64>(scaledPointsDev_, point_count);

    LOG(INFO) << "Map-" << id_ << " GPU edge readback (first " << checkEdges
              << " edges):";

    for (uint32_t i = 0; i < checkEdges; ++i) {
      const auto& e = gpuEdges[i];

      auto& p1 = gpuPts[e.p1_idx];
      auto& p2 = gpuPts[e.p2_idx];

      int64_t a = p1.y - p2.y;
      int64_t b = p2.x - p1.x;
      int64_t c = -(p1.x * a) - (p1.y * b);

      if (b < 0) {
        a = -a;
        b = -b;
        c = -c;
      }

      bool ok = (a == e.a) && (b == e.b) && (c == e.c);

      LOG(INFO) << "eid=" << e.eid << " p1=" << e.p1_idx << " p2=" << e.p2_idx
                << " GPU=(" << e.a << "," << e.b << "," << e.c << ")"
                << " CPU=(" << a << "," << b << "," << c << ")"
                << " match=" << (ok ? "YES" : "NO");
    }
  }
};

}  // namespace vk
}  // namespace rayjoin

#endif