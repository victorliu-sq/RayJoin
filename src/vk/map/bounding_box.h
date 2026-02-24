#ifndef RAYJOIN_MAP_BOUNDING_BOX_H
#define RAYJOIN_MAP_BOUNDING_BOX_H
#include "util/util.h"
namespace rayjoin {
namespace vk {
template <typename COORD_T>
struct BoundingBox{
  COORD_T min_x, min_y, max_x, max_y;
};

template <typename COORD_T>
inline BoundingBox<COORD_T> bboxInitHost() {
  BoundingBox<COORD_T> b{};
  b.min_x = std::numeric_limits<COORD_T>::max();
  b.min_y = std::numeric_limits<COORD_T>::max();
  b.max_x = std::numeric_limits<COORD_T>::lowest();
  b.max_y = std::numeric_limits<COORD_T>::lowest();
  return b;
}
}

}  // namespace rayjoin
#endif  // RAYJOIN_MAP_BOUNDING_BOX_H
