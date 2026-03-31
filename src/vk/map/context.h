#ifndef RAYJOIN_CONTEXT_H
#define RAYJOIN_CONTEXT_H

#include "../core/vk_global_context.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/map/bounding_box.h"
#include "vk/map/map.h"
#include "vk/map/scaling.h"

namespace rayjoin {
namespace vk {

// ===========================================================
// Concept support for Context
template<typename POINT_COORD_T, typename EDGE_COEFFICIENT_T>
  requires PointCoordType<POINT_COORD_T>
class Context;

// Pass a sinlge type arugment Context to check whether it satisfies the condition
template<typename T>
struct is_context : std::false_type {};

template<typename POINT_COORD_T, typename EDGE_COEFFICIENT_T>
struct is_context<Context<POINT_COORD_T, EDGE_COEFFICIENT_T>> : std::true_type {};

template<typename T>
concept ContextType = is_context<T>::value;
// ===========================================================

template<typename POINT_COORD_T, typename EDGE_COEFFICIENT_T = __int128>
  requires PointCoordType<POINT_COORD_T>
class Context {
 public:
  using coord_t = POINT_COORD_T;
  using coefficient_t = EDGE_COEFFICIENT_T;

  using scaling_t = Scaling<coord_t>;
  using internal_coord_t = scaling_t::internal_coord_t;
  using planar_graph_t = PlanarGraph<coord_t>;
  using map_t = Map<internal_coord_t, coefficient_t>;
  using bounding_box_t = BoundingBox<coord_t>;

  Context() = delete;

  ~Context() {
    // 1) Destroy GPU users first (Maps hold VMA allocations)
    for (auto& m: maps_) {
      m.reset();
    }

    // 2) Now it's safe to destroy VMA + device
    // destroyVkComputeContext(vk_);
    //
    // // 3) Finally instance
    // if (instance_) {
    //   vkDestroyInstance(instance_, nullptr);
    //   instance_ = VK_NULL_HANDLE;
    // }
  }

  explicit Context(const std::array<std::shared_ptr<planar_graph_t>, 2>& planar_graphs) : planar_graphs_(planar_graphs) {
    for (auto pgraph: planar_graphs) {
      if (pgraph != nullptr) {
        auto& bb = pgraph->bb;
        bb_.min_x = std::min(bb_.min_x, bb.min_x);
        bb_.max_x = std::max(bb_.max_x, bb.max_x);
        bb_.min_y = std::min(bb_.min_y, bb.min_y);
        bb_.max_y = std::max(bb_.max_y, bb.max_y);
      }
    }

    // Scaling Object
    scaling_ = Scaling<coord_t>(bb_);
#ifndef NDEBUG
    scaling_.DebugPrint();
#endif
    LOG(INFO) << "Bounding Box, Bottom-left: (" << bb_.min_x << ", " << bb_.min_y << "), Top-right: (" << bb_.max_x << ", " << bb_.max_y << ")";

    auto internal_min_x = scaling_.ScaleX(bb_.min_x);
    auto internal_min_y = scaling_.ScaleY(bb_.min_y);
    auto internal_max_x = scaling_.ScaleX(bb_.max_x);
    auto internal_max_y = scaling_.ScaleY(bb_.max_y);
    LOG(INFO) << "Scaled Bounding Box, Bottom-left: (" << internal_min_x << ", " << internal_min_y << "), Top-right: (" << internal_max_x << ","
              << internal_max_y << ")";
    LOG(INFO) << "Unscaled Bounding Box, Bottom-left: (" << scaling_.UnscaleX(internal_min_x) << ", " << scaling_.UnscaleY(internal_min_y)
              << "), Top-right: (" << scaling_.UnscaleX(internal_max_x) << ", " << scaling_.UnscaleY(internal_max_y) << ")";

    // char result[PATH_MAX];
    // ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    // const char* path;
    // if (count != -1) {
    //   path = dirname(result);
    // }
    // exec_root = std::string(path);
  }

  void InitMaps() {
    for (size_t im = 0; im < planar_graphs_.size(); im++) {
      auto pgraph = planar_graphs_[im];

      if (pgraph != nullptr) {
        // auto map = std::make_shared<map_t>(im, vk_);
        auto map = std::make_shared<map_t>(im);

        assert(pgraph != nullptr);
        map->LoadFrom(scaling_, *pgraph);
        maps_[im] = map;
      }
    }
  }

  std::shared_ptr<map_t> get_map(int mapno) { return maps_[mapno]; }

  // For MapOverlayRT::Init
  size_t get_edge_num() const {
    size_t n_edges = 0;
    FOR2 { n_edges += this->maps_[im]->get_edges_num(); }
    LOG(INFO) << "Total Number of Edges From Two Maps: " << n_edges << ".";
    return n_edges;
  }

  // VkInstance getVkInstance() const {
  //   return instance_;
  // }

 private:
  std::array<std::shared_ptr<planar_graph_t>, 2> planar_graphs_;
  std::array<std::shared_ptr<map_t>, 2> maps_;
  bounding_box_t bb_;
  scaling_t scaling_;
  // std::string exec_root;  // folder of binary
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_CONTEXT_H
