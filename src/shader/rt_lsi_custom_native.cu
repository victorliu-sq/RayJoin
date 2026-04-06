#include <cuda.h>
#include <cuda_runtime.h>
#include <optix.h>
#include <optix_device.h>

#include "shader/config.h"
#include "shader/launch_parameters_native.h"
#include "shader/lsi_native.h"
#include "util/helpers.h"
#include "util/util.h"
#include "util/vec_math.h"

enum { SURFACE_RAY_TYPE = 0, RAY_TYPE_COUNT };

extern "C" __constant__ rayjoin::LaunchParamsLSINative params;

extern "C" __global__ void __intersection__lsi_custom_native() {
  using xsect_t = typename rayjoin::LaunchParamsLSINative::xsect_t;
  using edge_t = typename rayjoin::LaunchParamsLSINative::edge_t;
  using point_t = typename rayjoin::LaunchParamsLSINative::point_t;
  using coord_t = typename rayjoin::LaunchParamsLSINative::coord_t;

  const auto prim_idx = optixGetPrimitiveIndex();
  const auto query_eid = optixGetPayload_0();
  const auto query_map_id = params.query_map_id;

  const auto& query_e = params.query_edges[query_eid];
  const auto& query_e_p1 = params.query_points[query_e.p1_idx];
  const auto& query_e_p2 = params.query_points[query_e.p2_idx];

  const auto begin_eid = params.eid_range[prim_idx].first;
  const auto end_eid = params.eid_range[prim_idx].second;

  for (auto base_eid = begin_eid; base_eid < end_eid; ++base_eid) {
    const auto& base_e = params.base_edges[base_eid];
    const auto& base_e_p1 = params.base_points[base_e.p1_idx];
    const auto& base_e_p2 = params.base_points[base_e.p2_idx];

#ifndef NDEBUG
    atomicAdd(params.n_tests, 1u);
#endif

    coord_t xq, yq;
    if (rayjoin::dev::intersect_test_with_point_native<edge_t, edge_t, point_t, coord_t>(
            query_e, query_e_p1, query_e_p2,
            base_e, base_e_p1, base_e_p2,
            xq, yq)) {
      xsect_t xsect;
      xsect.x = xq;
      xsect.y = yq;

      if (query_map_id == 0) {
        xsect.eid[0] = query_eid;
        xsect.eid[1] = base_eid;
      } else {
        xsect.eid[0] = base_eid;
        xsect.eid[1] = query_eid;
      }

      xsect.mid_point_polygon_id = DONTKNOW;
      params.xsects.Append(xsect);
    }
  }
}

extern "C" __global__ void __raygen__lsi_custom_native() {
  const auto& edges = params.query_edges;

  const float tmin = 0.0f;
  const float tmax = 1.0f;

  for (unsigned int eid = OPTIX_TID_1D; eid < edges.size(); eid += OPTIX_TOTAL_THREADS_1D) {
    const auto& e = edges[eid];
    auto p1 = params.query_points[e.p1_idx];
    auto p2 = params.query_points[e.p2_idx];

    if (e.b == 0.0) {
      if (p1.y > p2.y) {
        SWAP(p1, p2);
      }
    } else {
      if (p1.x > p2.x) {
        SWAP(p1, p2);
      }
    }

    float3 ray_origin = {
        static_cast<float>(p1.x),
        static_cast<float>(p1.y),
        0.0f};

    float3 ray_dir = {
        static_cast<float>(p2.x - p1.x),
        static_cast<float>(p2.y - p1.y),
        0.0f};

    optixTrace(params.traversable,
               ray_origin,
               ray_dir,
               tmin,
               tmax,
               0.0f,
               OptixVisibilityMask(255),
               OPTIX_RAY_FLAG_NONE,
               SURFACE_RAY_TYPE,
               RAY_TYPE_COUNT,
               SURFACE_RAY_TYPE,
               eid);
  }
}