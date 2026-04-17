#include <cuda.h>
#include <cuda_runtime.h>
#include <optix.h>
#include <optix_device.h>

#include "config.h"
#include "launch_parameters_native.h"
#include "util/helpers.h"
#include "util/int128_intrinsics.h"
#include "util/rational.h"
#include "util/util.h"
#include "util/vec_math.h"

enum { SURFACE_RAY_TYPE = 0, RAY_TYPE_COUNT };
extern "C" __constant__ rayjoin::LaunchParamsPIPNative params;

extern "C" __global__ void __intersection__pip_custom_native() {
  auto point_idx = optixGetPayload_0();
  double best_y;
  uint2 best_y_storage{optixGetPayload_1(), optixGetPayload_2()};
  auto prim_idx = optixGetPrimitiveIndex();
  auto query_map_id = params.query_map_id;
  const auto& src_p = params.query_points[point_idx];
  auto x_src_p = src_p.x;
  auto y_src_p = src_p.y;
  auto begin_eid = params.eid_range[prim_idx].first;
  auto end_eid = params.eid_range[prim_idx].second;

  unpack64(best_y_storage.x, best_y_storage.y, &best_y);

#ifndef NDEBUG
  params.hit_count[point_idx] += end_eid - begin_eid;
#endif

  float t = std::numeric_limits<float>::max();
  rayjoin::index_t best_e_eid = optixGetPayload_3();
  bool report_xsect = false;

  for (auto eid = begin_eid; eid < end_eid; ++eid) {
    const auto& e = params.base_map_edges[eid];
    const auto& p1 = params.base_map_points[e.p1_idx];
    const auto& p2 = params.base_map_points[e.p2_idx];
    auto x_min = min(p1.x, p2.x);
    auto x_max = max(p1.x, p2.x);

    if (x_src_p < x_min || x_src_p > x_max || x_src_p == ((query_map_id == 0) ? x_min : x_max)) {
      continue;
    }

    assert(e.b != 0);

    auto xsect_y = (-e.a * x_src_p - e.c) / e.b;
    auto diff_y = y_src_p - xsect_y;

    if (diff_y == 0) diff_y = (query_map_id == 0 ? -e.a : e.a);
    if (diff_y == 0) diff_y = (query_map_id == 0 ? -e.b : e.b);

    if (diff_y > 0) continue;

    if (xsect_y > best_y) {
#ifndef NDEBUG
      params.fail_update_count[point_idx]++;
#endif
      continue;
    }

    // if (xsect_y == best_y) {
    //   auto& best_e = params.base_map_edges[best_e_eid];
    //   auto current_e_slope = e.a / e.b;
    //   auto best_e_slope = best_e.a / best_e.b;

    //   if (current_e_slope != best_e_slope) {
    //     bool flag = current_e_slope > best_e_slope;
    //     if ((query_map_id && !flag) || (flag && !query_map_id)) continue;
    //   } else {
    //     if (eid >= best_e_eid) continue;
    //   }
    // }
    if (xsect_y == best_y) {
      if (eid >= best_e_eid) continue;
    }

    t = std::min(t, static_cast<float>(xsect_y - y_src_p));
    best_y = xsect_y;
    best_e_eid = eid;
    report_xsect = true;
  }

  if (report_xsect) {
    pack64(&best_y, best_y_storage.x, best_y_storage.y);
    optixSetPayload_1(best_y_storage.x);
    optixSetPayload_2(best_y_storage.y);
    optixSetPayload_3(best_e_eid);
    optixReportIntersection(t, 0);
  }

#ifndef NDEBUG
  params.closer_count[point_idx]++;
  params.last_update_count[point_idx] = params.hit_count[point_idx];
#endif
}

extern "C" __global__ void __raygen__pip_custom_native() {
  float3 ray_dir = {0, 1, 0};
  const auto& query_points = params.query_points;

  for (unsigned int point_idx = OPTIX_TID_1D; point_idx < query_points.size(); point_idx += OPTIX_TOTAL_THREADS_1D) {
    const auto& p = query_points[point_idx];
    const float x = static_cast<float>(p.x);
    const float y = static_cast<float>(p.y);
    const float tmin = 0.0f;
    const float tmax = RAY_TMAX;

    double best_y = std::numeric_limits<double>::infinity();
    uint2 best_y_storage;
    pack64(&best_y, best_y_storage.x, best_y_storage.y);
    rayjoin::index_t best_e_eid = std::numeric_limits<rayjoin::index_t>::max();

    float3 ray_origin = {x, y, 0};

    optixTrace(params.traversable,
               ray_origin,
               ray_dir,
               tmin,
               tmax,
               0,
               OptixVisibilityMask(255),
               OPTIX_RAY_FLAG_NONE,
               SURFACE_RAY_TYPE,
               RAY_TYPE_COUNT,
               SURFACE_RAY_TYPE,
               point_idx,
               best_y_storage.x,
               best_y_storage.y,
               best_e_eid);

    double traced_best_y;
    unpack64(best_y_storage.x, best_y_storage.y, &traced_best_y);

    params.closest_eids[point_idx] = best_e_eid;
    params.best_ys[point_idx] = traced_best_y;
  }
}
