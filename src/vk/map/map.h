#ifndef RAYJOIN_MAP_H
#define RAYJOIN_MAP_H

#include "vk/common/vk_context.h"
#include "vk/map/scale_points_d2_i64.h"
#include "vk/map/scaling.h"

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
  }

 private:
  int id_;
  VkComputeContext vk_;

  // Step1 pipeline
  ScalePointsPassD2I64 scale_pass_;
  // Output of step1 (device buffer with DstPointI64)
  AllocBuf gpu_points_{};
  uint32_t point_count_ = 0;

  bool scale_pass_inited_ = false;
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_H
