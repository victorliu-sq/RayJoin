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
  const auto point_idx = optixGetPayload_0();
  const auto prim_idx = optixGetPrimitiveIndex();
  const auto query_map_id = params.query_map_id;

  const auto& src_p = params.query_points[point_idx];
  const auto x_src_p = src_p.x;
  const auto y_src_p = src_p.y;

  const auto begin_eid = params.eid_range[prim_idx].first;
  const auto end_eid = params.eid_range[prim_idx].second;

#ifndef NDEBUG
  params.hit_count[point_idx] += end_eid - begin_eid;
#endif

  for (auto eid = begin_eid; eid < end_eid; ++eid) {
    const auto& e = params.base_map_edges[eid];
    const auto& p1 = params.base_map_points[e.p1_idx];
    const auto& p2 = params.base_map_points[e.p2_idx];

    const auto x_min = min(p1.x, p2.x);
    const auto x_max = max(p1.x, p2.x);

    if (x_src_p < x_min || x_src_p > x_max || x_src_p == ((query_map_id == 0) ? x_min : x_max)) {
      continue;
    }

    assert(e.b != 0);

    double xsect_y = (-e.a * x_src_p - e.c) / e.b;
    auto diff_y = y_src_p - xsect_y;

    if (diff_y == 0) diff_y = (query_map_id == 0 ? -e.a : e.a);
    if (diff_y == 0) diff_y = (query_map_id == 0 ? -e.b : e.b);

    if (diff_y > 0) continue;

    const float t = static_cast<float>(xsect_y - y_src_p);

    uint2 y_storage;
    pack64(&xsect_y, y_storage.x, y_storage.y);

    optixReportIntersection(t, 0, static_cast<unsigned int>(eid), y_storage.x, y_storage.y);
  }
}

extern "C" __global__ void __anyhit__pip_custom_native() {
  const auto point_idx = optixGetPayload_0();

  double best_y;
  uint2 best_y_storage{optixGetPayload_1(), optixGetPayload_2()};
  unpack64(best_y_storage.x, best_y_storage.y, &best_y);

  rayjoin::index_t best_e_eid = optixGetPayload_3();

  const auto candidate_eid = static_cast<rayjoin::index_t>(optixGetAttribute_0());
  const uint2 candidate_y_storage{optixGetAttribute_1(), optixGetAttribute_2()};

  double candidate_y;
  unpack64(candidate_y_storage.x, candidate_y_storage.y, &candidate_y);

  bool take = false;

  if (candidate_y > best_y) {
#ifndef NDEBUG
    params.fail_update_count[point_idx]++;
#endif
    optixIgnoreIntersection();
    return;
  }

  if (best_e_eid == std::numeric_limits<rayjoin::index_t>::max()) {
    take = true;
  } else if (candidate_y < best_y) {
    take = true;
  } else {  // candidate_y == best_y
    const auto& e = params.base_map_edges[candidate_eid];
    const auto& best_e = params.base_map_edges[best_e_eid];

    const auto current_e_slope = e.a / e.b;
    const auto best_e_slope = best_e.a / best_e.b;

    if (current_e_slope != best_e_slope) {
      const bool flag = current_e_slope > best_e_slope;
      if (!((params.query_map_id && !flag) || (flag && !params.query_map_id))) {
        take = true;
      }
    } else {
      if (candidate_eid < best_e_eid) {
        take = true;
      }
    }
  }

  if (take) {
    optixSetPayload_1(candidate_y_storage.x);
    optixSetPayload_2(candidate_y_storage.y);
    optixSetPayload_3(static_cast<unsigned int>(candidate_eid));
  }

#ifndef NDEBUG
  params.closer_count[point_idx]++;
  params.last_update_count[point_idx] = params.hit_count[point_idx];
#endif

  optixIgnoreIntersection();
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
