#ifndef RAYJOIN_MAP_BOUNDING_BOX_H
#define RAYJOIN_MAP_BOUNDING_BOX_H
// #include "util/util.h"
#include <limits>

namespace rayjoin {
namespace vk {
template <typename COORD_T>
struct BoundingBox {
  COORD_T min_x, min_y, max_x, max_y;
  // for double, 4 * 8 = 32 bytes
  BoundingBox()
      : min_x(std::numeric_limits<COORD_T>::max()),
        min_y(std::numeric_limits<COORD_T>::max()),
        max_x(std::numeric_limits<COORD_T>::lowest()),
        max_y(std::numeric_limits<COORD_T>::lowest()) {}
};
}  // namespace vk

}  // namespace rayjoin
#endif  // RAYJOIN_MAP_BOUNDING_BOX_H
