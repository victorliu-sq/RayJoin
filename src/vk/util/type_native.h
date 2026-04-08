#ifndef RAYJOIN_TYPE_NATIVE_H
#define RAYJOIN_TYPE_NATIVE_H

#include "vk/util/type_traits.h"

namespace rayjoin::vk {
// template<EdgeCoeffType COEFF_T>
// struct EdgeNS {
//   COEFF_T a;
//   COEFF_T b;
//   COEFF_T c;
//
//   uint64_t eid;
//   uint64_t p1_idx;
//   uint64_t p2_idx;
//   uint64_t left_polygon_id;
//   uint64_t right_polygon_id;
// };
//
// template<typename POINT_COORD_T>
//   requires PointCoordType<POINT_COORD_T>
// struct Intersection {
//   POINT_COORD_T x;
//   POINT_COORD_T y;
//
//   uint64_t eid0;
//   uint64_t eid1;
//
//   uint mid_point_polygon_id = 0;
//   uint pad;
// };

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

template<typename POINT_COORD_T>
  requires PointCoordType<POINT_COORD_T>
struct IntersectionNS {
  POINT_COORD_T x;
  POINT_COORD_T y;

  index_t eid0;
  index_t eid1;

  polygon_id_t mid_point_polygon_id = DONTKNOW;
};

}  // namespace rayjoin::vk

#endif  // RAYJOIN_TYPE_NATIVE_H
