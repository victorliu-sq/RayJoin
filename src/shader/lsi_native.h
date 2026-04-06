#ifndef ALGO_LSI_NATIVE_H
#define ALGO_LSI_NATIVE_H

#include "config.h"
#include "util/util.h"

namespace rayjoin {
namespace dev {

template<typename COORD_T>
struct IntersectionNative {
  COORD_T x, y;
  index_t eid[2];
  polygon_id_t mid_point_polygon_id;

  DEV_HOST IntersectionNative() : x(0), y(0), eid{0, 0}, mid_point_polygon_id(DONTKNOW) {}
};

template<typename POINT_T>
DEV_INLINE bool point_equals_native(const POINT_T& a, const POINT_T& b) {
  return a.x == b.x && a.y == b.y;
}

template<typename EDGE_T, typename COORD_T>
DEV_INLINE COORD_T tie_break_neg_native(const EDGE_T& e) {
  if (e.a != COORD_T(0)) return -e.a;
  if (e.b != COORD_T(0)) return -e.b;
  return COORD_T(0);
}

template<typename EDGE_T, typename COORD_T>
DEV_INLINE COORD_T tie_break_pos_native(const EDGE_T& e) {
  if (e.a != COORD_T(0)) return e.a;
  if (e.b != COORD_T(0)) return e.b;
  return COORD_T(0);
}

template<typename COORD_T>
DEV_INLINE COORD_T min2_native(COORD_T a, COORD_T b) {
  return (a < b) ? a : b;
}

template<typename COORD_T>
DEV_INLINE COORD_T max2_native(COORD_T a, COORD_T b) {
  return (a > b) ? a : b;
}

template<typename COORD_T>
DEV_INLINE COORD_T min4_native(COORD_T a, COORD_T b, COORD_T c, COORD_T d) {
  return min2_native(min2_native(a, b), min2_native(c, d));
}

template<typename COORD_T>
DEV_INLINE COORD_T max4_native(COORD_T a, COORD_T b, COORD_T c, COORD_T d) {
  return max2_native(max2_native(a, b), max2_native(c, d));
}

template<typename EDGE_T, typename POINT_T, typename COORD_T>
DEV_INLINE COORD_T subedge_native(const POINT_T& p, const EDGE_T& e) {
  return p.x * e.a + p.y * e.b + e.c;
}

template<typename EDGE_T1, typename EDGE_T2, typename POINT_T, typename COORD_T>
DEV_INLINE bool intersect_test_native(
    const EDGE_T1& e1, const POINT_T& e1_p1, const POINT_T& e1_p2, const EDGE_T2& e2, const POINT_T& e2_p1, const POINT_T& e2_p2) {
  COORD_T e2_p1_agst_e1 = subedge_native<EDGE_T1, POINT_T, COORD_T>(e2_p1, e1);
  COORD_T e2_p2_agst_e1 = subedge_native<EDGE_T1, POINT_T, COORD_T>(e2_p2, e1);
  COORD_T e1_p1_agst_e2 = subedge_native<EDGE_T2, POINT_T, COORD_T>(e1_p1, e2);
  COORD_T e1_p2_agst_e2 = subedge_native<EDGE_T2, POINT_T, COORD_T>(e1_p2, e2);

  if (e1_p1_agst_e2 == COORD_T(0)) e1_p1_agst_e2 = tie_break_neg_native<EDGE_T2, COORD_T>(e2);
  if (e1_p1_agst_e2 == COORD_T(0)) return false;

  if (e1_p2_agst_e2 == COORD_T(0)) e1_p2_agst_e2 = tie_break_neg_native<EDGE_T2, COORD_T>(e2);
  if (e1_p2_agst_e2 == COORD_T(0)) return false;

  if ((e1_p1_agst_e2 > COORD_T(0) && e1_p2_agst_e2 > COORD_T(0)) || (e1_p1_agst_e2 < COORD_T(0) && e1_p2_agst_e2 < COORD_T(0))) {
    return false;
  }

  if (e2_p1_agst_e1 == COORD_T(0)) e2_p1_agst_e1 = tie_break_pos_native<EDGE_T1, COORD_T>(e1);
  if (e2_p1_agst_e1 == COORD_T(0)) return false;

  if (e2_p2_agst_e1 == COORD_T(0)) e2_p2_agst_e1 = tie_break_pos_native<EDGE_T1, COORD_T>(e1);
  if (e2_p2_agst_e1 == COORD_T(0)) return false;

  if ((e2_p1_agst_e1 > COORD_T(0) && e2_p2_agst_e1 > COORD_T(0)) || (e2_p1_agst_e1 < COORD_T(0) && e2_p2_agst_e1 < COORD_T(0))) {
    return false;
  }

  if ((point_equals_native(e1_p1, e2_p1) && point_equals_native(e1_p2, e2_p2)) ||
      (point_equals_native(e1_p1, e2_p2) && point_equals_native(e1_p2, e2_p1))) {
    return false;
  }

  return true;
}

template<typename EDGE_T1, typename EDGE_T2, typename POINT_T, typename COORD_T>
DEV_INLINE bool intersect_test_with_point_native(const EDGE_T1& e1,
                                                 const POINT_T& e1_p1,
                                                 const POINT_T& e1_p2,
                                                 const EDGE_T2& e2,
                                                 const POINT_T& e2_p1,
                                                 const POINT_T& e2_p2,
                                                 COORD_T& xsect_x,
                                                 COORD_T& xsect_y) {
  xsect_x = COORD_T(0);
  xsect_y = COORD_T(0);

  if (!intersect_test_native<EDGE_T1, EDGE_T2, POINT_T, COORD_T>(e1, e1_p1, e1_p2, e2, e2_p1, e2_p2)) {
    return false;
  }

  COORD_T denom = e1.a * e2.b - e2.a * e1.b;
  if (denom == COORD_T(0)) return false;

  xsect_x = (e2.c * e1.b - e1.c * e2.b) / denom;
  xsect_y = (e2.a * e1.c - e1.a * e2.c) / denom;

  COORD_T t;

  t = min4_native(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
  if (xsect_x < t) xsect_x = t;

  t = max4_native(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
  if (xsect_x > t) xsect_x = t;

  t = min4_native(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
  if (xsect_y < t) xsect_y = t;

  t = max4_native(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
  if (xsect_y > t) xsect_y = t;

  return true;
}

}  // namespace dev
}  // namespace rayjoin

#endif
