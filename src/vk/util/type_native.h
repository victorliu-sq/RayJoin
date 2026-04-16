#ifndef RAYJOIN_TYPE_NATIVE_H
#define RAYJOIN_TYPE_NATIVE_H

#include "shader/config.h"
#include "vk/util/type_traits.h"

namespace rayjoin::vk {

// ===============================================================
// Edge
template<typename COEFF_T>
struct EdgeNS {
  COEFF_T a;
  COEFF_T b;
  COEFF_T c;

  index_t eid;
  index_t p1_idx;
  index_t p2_idx;
  index_t left_polygon_id;
  index_t right_polygon_id;
};

// ===============================================================
struct EidRange {
  uint32_t first = 0;
  uint32_t second = 0;
};

// ===============================================================
// xsect
template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
struct IntersectionNS {
  POINT_COORD_T x;
  POINT_COORD_T y;

  index_t eid0;
  index_t eid1;

  polygon_id_t mid_point_polygon_id = DONTKNOW;
};

template<typename T>
struct is_intersection_ns : std::false_type {};

template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
struct is_intersection_ns<IntersectionNS<POINT_COORD_T>> : std::true_type {};

template<typename T>
inline constexpr bool is_intersection_ns_v = is_intersection_ns<T>::value;

template<typename T>
concept IntersectionNSType = is_intersection_ns_v<T>;

}  // namespace rayjoin::vk

#endif  // RAYJOIN_TYPE_NATIVE_H
