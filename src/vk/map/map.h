#ifndef RAYJOIN_MAP_H
#define RAYJOIN_MAP_H

#include "edge_init_pass_i64.h"
#include "edge_init_pass_i64_dev_addr.h"
#include "edge_init_pass_i64_raii.h"
#include "glog/logging.h"
#include "planar_graph.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/map/gpu_edge_types.h"
#include "vk/map/scale_points_d2_i64.h"
#include "vk/map/scaling.h"
#include "vk/map/vk_debug_readback.h"

namespace rayjoin {
namespace vk {

template <typename INTERNAL_COORD_T, typename COEFFICIENT_T>
class Map {
 public:
  using internal_coord_t = INTERNAL_COORD_T;
  using coefficient_t = COEFFICIENT_T;
  using point_t = Vec2<internal_coord_t>;

  Map() = delete;
  // explicit Map(int id) : id_(id) {}
  Map(int id, const VkComputeContext& ctx) : id_(id), vk_(ctx) {}

  ~Map() {
    // if (edge_pass_inited_) {
    //   edge_pass_.destroy();
    //   edge_pass_inited_ = false;
    // }

    if (scale_pass_inited_) {
      scale_pass_.destroy();
      scale_pass_inited_ = false;
    }
  }

  template <typename SRC_COORD_T>
  void LoadFrom(const Scaling<SRC_COORD_T, INTERNAL_COORD_T>& scaling,
                const PlanarGraph<SRC_COORD_T>& pgraph) {
    LOG(INFO) << "Init Map-" << id_ << " From PGraphs";
    // Step1 only: scale points on GPU
    point_count_ = static_cast<uint32_t>(pgraph.points.size());
    if (point_count_ == 0) {
      LOG(WARNING) << "Map-" << id_ << ": empty planar graph points";
      return;
    }

    // Init compute pass once
    if (!scale_pass_inited_) {
      // SHADER_DIR comes from your CMake: SHADER_DIR="${SPV_OUTPUT_DIR}"
      std::string spvPath =
          std::string(SHADER_DIR) + "/scale_points_d2_i64.spv";
      scale_pass_.init(vk_, spvPath.c_str());
      scale_pass_inited_ = true;
    }

    // Prepare buffers sized for current count
    scale_pass_.prepareBuffers(point_count_);

    // Convert src points to aligned SrcPointD array
    std::vector<SrcPointD> src(point_count_);
    for (uint32_t i = 0; i < point_count_; ++i) {
      src[i].x = static_cast<double>(pgraph.points[i].x);
      src[i].y = static_cast<double>(pgraph.points[i].y);
    }

    // Run compute
    scale_pass_.run(src.data(), point_count_, (double) scaling.rx(),
                    (double) scaling.ry(), (double) scaling.deltax(),
                    (double) scaling.deltay());

    // Keep GPU output buffer for next step (edge equation kernel later)
    gpu_points_ = scale_pass_.dstBuffer();

    LOG(INFO) << "Map-" << id_ << ": scaled " << point_count_
              << " points on GPU";

    // DEBUG: read back first few points
    {
      uint32_t checkCount = std::min<uint32_t>(point_count_, 10);

      auto gpuPts =
          readBackBuffer<DstPointI64>(vk_, scale_pass_.dstBuffer(), checkCount);

      LOG(INFO) << "Map-" << id_ << " GPU readback (first " << checkCount
                << " points):";

      for (uint32_t i = 0; i < checkCount; ++i) {
        auto& srcp = pgraph.points[i];

        int64_t gpu_x = gpuPts[i].x;
        int64_t gpu_y = gpuPts[i].y;

        // CPU reference
        int64_t cpu_x = scaling.ScaleX(srcp.x);
        int64_t cpu_y = scaling.ScaleY(srcp.y);

        // Unscale GPU back to double for sanity
        double unscaled_x = scaling.UnscaleX(gpu_x);
        double unscaled_y = scaling.UnscaleY(gpu_y);

        LOG(INFO) << "i=" << i << " src=(" << srcp.x << "," << srcp.y << ")"
                  << " gpu=(" << gpu_x << "," << gpu_y << ")"
                  << " cpu=(" << cpu_x << "," << cpu_y << ")"
                  << " unscaled=(" << unscaled_x << "," << unscaled_y << ")";
      }
    }

    // ----- Step2: initialize edges on GPU -----
    // chain_count_ = static_cast<uint32_t>(pgraph.chains.size());
    // if (chain_count_ == 0) {
    //   LOG(WARNING) << "Map-" << id_ << ": no chains";
    //   return;
    // }
    //
    // edge_count_ = point_count_ - chain_count_;
    // LOG(INFO) << "Map-" << id_ << ": chains=" << chain_count_
    //           << " edges=" << edge_count_;
    //
    // if (!edge_pass_inited_) {
    //   // std::string spvPath = std::string(SHADER_DIR) +
    //   "/edge_init_i64.spv"; std::string spvPath = std::string(SHADER_DIR) +
    //   "/edge_init_i64_dev_addr.spv"; edge_pass_.init(vk_, spvPath.c_str());
    //   edge_pass_inited_ = true;
    // }
    //
    // edge_pass_.prepareBuffers(chain_count_, point_count_);
    //
    // // Build GPU chain array (only needed fields)
    // std::vector<GpuChain> chainsGpu(chain_count_);
    // for (uint32_t i = 0; i < chain_count_; ++i) {
    //   chainsGpu[i].left_polygon_id =
    //       static_cast<int32_t>(pgraph.chains[i].left_polygon_id);
    //   chainsGpu[i].right_polygon_id =
    //       static_cast<int32_t>(pgraph.chains[i].right_polygon_id);
    // }
    //
    // // Build row_index (uint32)
    // std::vector<GpuIndex> rowGpu(chain_count_ + 1);
    // for (uint32_t i = 0; i < chain_count_ + 1; ++i) {
    //   rowGpu[i] = static_cast<GpuIndex>(pgraph.row_index[i]);
    // }
    //
    // // Points buffer from scale pass
    // const AllocBuf& pointsDev = scale_pass_.dstBuffer();
    //
    // edge_pass_.run(pointsDev, chainsGpu, rowGpu);
    //
    // LOG(INFO) << "Map-" << id_ << ": initialized " << edge_pass_.numEdges()
    //           << " edges on GPU";

    chain_count_ = static_cast<uint32_t>(pgraph.chains.size());

    if (chain_count_ == 0) {
      LOG(WARNING) << "Map-" << id_ << ": no chains";
      return;
    }

    edge_count_ = point_count_ - chain_count_;

    LOG(INFO) << "Map-" << id_ << ": chains=" << chain_count_
              << " edges=" << edge_count_;

    std::vector<GpuChain> chainsGpu(chain_count_);

    for (uint32_t i = 0; i < chain_count_; ++i) {
      chainsGpu[i].left_polygon_id =
          static_cast<int32_t>(pgraph.chains[i].left_polygon_id);

      chainsGpu[i].right_polygon_id =
          static_cast<int32_t>(pgraph.chains[i].right_polygon_id);
    }

    std::vector<GpuIndex> rowGpu(chain_count_ + 1);

    for (uint32_t i = 0; i < chain_count_ + 1; ++i) {
      rowGpu[i] = static_cast<GpuIndex>(pgraph.row_index[i]);
    }

    const AllocBuf& pointsDev = scale_pass_.dstBuffer();

    std::string spvPath =
        std::string(SHADER_DIR) + "/edge_init_i64_dev_addr.spv";

    edge_pass_ = std::make_unique<EdgeInitPassI64RAII>(
        vk_, spvPath.c_str(), pointsDev, chainsGpu, rowGpu);

    edge_pass_->run();

    LOG(INFO) << "Map-" << id_ << ": initialized " << edge_count_
              << " edges on GPU";
  }

  size_t get_points_num() const { return point_count_; }

  size_t get_edges_num() const { return edge_count_; }

 private:
  int id_;
  VkComputeContext vk_;

  // Step1 pipeline
  ScalePointsPassD2I64 scale_pass_;
  // Output of step1 (device buffer with DstPointI64)
  AllocBuf gpu_points_{};
  uint32_t point_count_ = 0;

  // Step2 Pipeline
  bool scale_pass_inited_ = false;

  // EdgeInitPassI64 edge_pass_;
  // EdgeInitPassI64DevAddr edge_pass_;
  // bool edge_pass_inited_ = false;

  // Step2 pipeline (RAII)
  std::unique_ptr<EdgeInitPassI64RAII> edge_pass_;

  uint32_t chain_count_ = 0;
  uint32_t edge_count_ = 0;
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_H
