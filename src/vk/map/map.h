#ifndef RAYJOIN_MAP_H
#define RAYJOIN_MAP_H

namespace rayjoin {
namespace vk {

template <typename INTERNAL_COORD_T, typename COEFFICIENT_T>
class Map {
 public:
  using internal_coord_t = INTERNAL_COORD_T;
  using coefficient_t = COEFFICIENT_T;
  using point_t = Vec2<internal_coord_t>;

  Map() = delete;

  explicit Map(int id) : id_(id) {}


  template <typename SRC_COORD_T>
  void LoadFrom(const Scaling<SRC_COORD_T, INTERNAL_COORD_T>& scaling,
                const PlanarGraph<SRC_COORD_T>& pgraph) {
    LOG(INFO) << "Init Map-" << id_ << " From PGraphs";

  }

 private:
  int id_;
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_H
