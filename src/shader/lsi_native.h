#ifndef ALGO_LSI_NATIVE_H
#define ALGO_LSI_NATIVE_H

#include <cmath>

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

template<typename EDGE_T1, typename EDGE_T2, typename POINT_T, typename COORD_T>
DEV_INLINE bool intersect_test_native(
    const EDGE_T1& e1, const POINT_T& e1_p1, const POINT_T& e1_p2, const EDGE_T2& e2, const POINT_T& e2_p1, const POINT_T& e2_p2) {
#define SUBEDGE(p, e) ((p).x * (e).a + (p).y * (e).b + (e).c)

  COORD_T e2_p1_agst_e1 = SUBEDGE(e2_p1, e1);
  COORD_T e2_p2_agst_e1 = SUBEDGE(e2_p2, e1);
  COORD_T e1_p1_agst_e2 = SUBEDGE(e1_p1, e2);
  COORD_T e1_p2_agst_e2 = SUBEDGE(e1_p2, e2);

#undef SUBEDGE

  constexpr COORD_T eps = static_cast<COORD_T>(1e-15);

  if (fabs((double) e1_p1_agst_e2) <= eps) e1_p1_agst_e2 = -e2.a;
  if (fabs((double) e1_p1_agst_e2) <= eps) e1_p1_agst_e2 = -e2.b;
  if (fabs((double) e1_p1_agst_e2) <= eps) return false;

  if (fabs((double) e1_p2_agst_e2) <= eps) e1_p2_agst_e2 = -e2.a;
  if (fabs((double) e1_p2_agst_e2) <= eps) e1_p2_agst_e2 = -e2.b;
  if (fabs((double) e1_p2_agst_e2) <= eps) return false;

  if ((e1_p1_agst_e2 > 0 && e1_p2_agst_e2 > 0) || (e1_p1_agst_e2 < 0 && e1_p2_agst_e2 < 0)) {
    return false;
  }

  if (fabs((double) e2_p1_agst_e1) <= eps) e2_p1_agst_e1 = e1.a;
  if (fabs((double) e2_p1_agst_e1) <= eps) e2_p1_agst_e1 = e1.b;
  if (fabs((double) e2_p1_agst_e1) <= eps) return false;

  if (fabs((double) e2_p2_agst_e1) <= eps) e2_p2_agst_e1 = e1.a;
  if (fabs((double) e2_p2_agst_e1) <= eps) e2_p2_agst_e1 = e1.b;
  if (fabs((double) e2_p2_agst_e1) <= eps) return false;

  if ((e2_p1_agst_e1 > 0 && e2_p2_agst_e1 > 0) || (e2_p1_agst_e1 < 0 && e2_p2_agst_e1 < 0)) {
    return false;
  }

  const bool same_dir = e1_p1.x == e2_p1.x && e1_p1.y == e2_p1.y && e1_p2.x == e2_p2.x && e1_p2.y == e2_p2.y;

  const bool opp_dir = e1_p1.x == e2_p2.x && e1_p1.y == e2_p2.y && e1_p2.x == e2_p1.x && e1_p2.y == e2_p1.y;

  if (same_dir || opp_dir) {
    return false;
  }

  return true;
}

template<typename EDGE_T1, typename EDGE_T2, typename POINT_T, typename COORD_T>
DEV_INLINE bool intersect_test_native(const EDGE_T1& e1,
                                      const POINT_T& e1_p1,
                                      const POINT_T& e1_p2,
                                      const EDGE_T2& e2,
                                      const POINT_T& e2_p1,
                                      const POINT_T& e2_p2,
                                      COORD_T& xsect_x,
                                      COORD_T& xsect_y) {
  if (!intersect_test_native<EDGE_T1, EDGE_T2, POINT_T, COORD_T>(e1, e1_p1, e1_p2, e2, e2_p1, e2_p2)) {
    return false;
  }

  const COORD_T denom = e1.a * e2.b - e2.a * e1.b;
  constexpr COORD_T eps = static_cast<COORD_T>(1e-15);

  if (fabs((double) denom) <= eps) {
    return false;
  }

  const COORD_T numx = e2.c * e1.b - e1.c * e2.b;
  const COORD_T numy = e2.a * e1.c - e1.a * e2.c;

  xsect_x = numx / denom;
  xsect_y = numy / denom;

  auto t = MIN4(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
  if (xsect_x < t) xsect_x = t;

  t = MAX4(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
  if (xsect_x > t) xsect_x = t;

  t = MIN4(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
  if (xsect_y < t) xsect_y = t;

  t = MAX4(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
  if (xsect_y > t) xsect_y = t;

  return true;
}

}  // namespace dev
}  // namespace rayjoin

#endif
