#ifndef RAYJOIN_CONTEXT_NS_H
#define RAYJOIN_CONTEXT_NS_H

#include "map_ns.h"
#include "vk/map/bounding_box.h"
#include "vk/map/map.h"

namespace rayjoin {
namespace vk {

// ===========================================================
// Concept support for ContextNS
template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
class ContextNS;

template<typename T>
struct is_context_ns : std::false_type {};

template<typename POINT_COORD_T>
struct is_context_ns<ContextNS<POINT_COORD_T>> : std::true_type {};

template<typename T>
concept ContextNSType = is_context_ns<T>::value;
// ===========================================================

template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
class ContextNS {
 public:
  using coord_t = POINT_COORD_T;
  using coeff_t = POINT_COORD_T;
  using planar_graph_t = PlanarGraph<coord_t>;
  using map_t = MapNS<coord_t>;
  using bounding_box_t = BoundingBox<coord_t>;
  using point_t = Vec2<coord_t>;
  using edge_t = EdgeNS<coord_t>;

  ContextNS() = delete;

  explicit ContextNS(const std::array<std::shared_ptr<planar_graph_t>, 2>& planar_graphs) : planar_graphs_(planar_graphs) {
    bool first_bb = true;

    for (const auto& pgraph: planar_graphs_) {
      if (pgraph != nullptr) {
        const auto& bb = pgraph->bb;

        if (first_bb) {
          bb_ = bb;
          first_bb = false;
        } else {
          bb_.min_x = std::min(bb_.min_x, bb.min_x);
          bb_.max_x = std::max(bb_.max_x, bb.max_x);
          bb_.min_y = std::min(bb_.min_y, bb.min_y);
          bb_.max_y = std::max(bb_.max_y, bb.max_y);
        }
      }
    }

    if (!first_bb) {
      LOG(INFO) << "Bounding Box, Bottom-left: (" << bb_.min_x << ", " << bb_.min_y << "), Top-right: (" << bb_.max_x << ", " << bb_.max_y << ")";
    } else {
      LOG(INFO) << "ContextNS initialized with no planar graphs.";
    }
  }

  ~ContextNS() {
    for (auto& m: maps_) {
      m.reset();
    }
  }

  void LoadToDevice() {
    for (size_t im = 0; im < planar_graphs_.size(); ++im) {
      const auto& pgraph = planar_graphs_[im];

      if (pgraph != nullptr) {
        auto map = std::make_shared<map_t>(static_cast<int>(im));
        map->LoadFrom(*pgraph);
        maps_[im] = map;
      }
    }
  }

  std::shared_ptr<map_t> get_map(int mapno) { return maps_[mapno]; }
  std::shared_ptr<const map_t> get_map(int mapno) const { return maps_[mapno]; }

  // Used by WriteResults
  std::shared_ptr<planar_graph_t> get_planar_graph(int mapno) { return planar_graphs_[mapno]; }

  std::shared_ptr<const planar_graph_t> get_planar_graph(int mapno) const { return planar_graphs_[mapno]; }

  const bounding_box_t& get_bb() const { return bb_; }

  size_t get_edge_num() const {
    size_t n_edges = 0;

    for (size_t im = 0; im < maps_.size(); ++im) {
      if (maps_[im] != nullptr) {
        n_edges += maps_[im]->get_edges_num();
      }
    }

    LOG(INFO) << "Total Number of Edges From Two Maps: " << n_edges << ".";
    return n_edges;
  }

 private:
  std::array<std::shared_ptr<planar_graph_t>, 2> planar_graphs_{};
  std::array<std::shared_ptr<map_t>, 2> maps_{};
  bounding_box_t bb_{};
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_CONTEXT_NS_H
