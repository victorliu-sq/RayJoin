module pip_rt_ns_d64;

struct PointD {
  double x;
  double y;
};

struct EdgeD {
  double a;
  double b;
  double c;

  uint64_t eid;
  uint64_t p1_idx;
  uint64_t p2_idx;
  uint64_t left_polygon_id;
  uint64_t right_polygon_id;
};

struct EidRange {
  uint first;
  uint second;
};

struct LaunchParamsPIP {
  int query_map_id;
  uint query_point_count;
  uint _pad0;
  uint _pad1;
};

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure gScene;

[[vk::binding(1, 0)]]
StructuredBuffer<LaunchParamsPIP> gParamsBuf;

[[vk::binding(2, 0)]]
StructuredBuffer<EdgeD> gBaseEdges;

[[vk::binding(3, 0)]]
StructuredBuffer<PointD> gBasePoints;

[[vk::binding(4, 0)]]
StructuredBuffer<EidRange> gEidRanges;

[[vk::binding(5, 0)]]
StructuredBuffer<PointD> gQueryPoints;

[[vk::binding(6, 0)]]
RWStructuredBuffer<uint> gClosestEids;

[[vk::binding(7, 0)]]
RWStructuredBuffer<uint> gDebugCounter;

// -----------------------------------------------------------------------------
// Payload and hit attributes
// -----------------------------------------------------------------------------

struct Payload {
  uint pointIdx;
  uint bestEid;
  float bestT;
  uint _pad0;
};

struct HitAttr {
  uint baseEid;
  float candidateT;
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

double min2_d(double a, double b) {
  return (a < b) ? a : b;
}

double max2_d(double a, double b) {
  return (a > b) ? a : b;
}

double edgeSlope(EdgeD e) {
  // Same quantity used by the OptiX code
  return e.a / e.b;
}

// Returns whether candidate_e should replace best_e when both have same t.
// This mirrors the OptiX tie-break policy.
bool preferCandidateAtSameT(uint query_map_id, EdgeD candidate_e, EdgeD best_e) {
  double candidate_slope = edgeSlope(candidate_e);
  double best_slope = edgeSlope(best_e);
  bool flag = candidate_slope > best_slope;

  // OptiX:
  // if ((query_map_id && !flag) || (flag && !query_map_id)) continue;
  // meaning candidate is preferred iff NOT that condition.
  return !((query_map_id != 0 && !flag) || (flag && query_map_id == 0));
}

// -----------------------------------------------------------------------------
// Ray generation
// -----------------------------------------------------------------------------

[shader("raygeneration")]
void raygenMain() {
  uint point_idx = DispatchRaysIndex().x;
  LaunchParamsPIP params = gParamsBuf[0];

  if (point_idx >= params.query_point_count)
    return;

  uint old0;
  InterlockedAdd(gDebugCounter[0], 1, old0);  // raygen count
  gDebugCounter[4] = point_idx;               // last point

  PointD p = gQueryPoints[point_idx];

  RayDesc ray;
  ray.Origin = float3(float(p.x), float(p.y), 0.0);
  ray.Direction = float3(0.0, 1.0, 0.0);
  ray.TMin = 0.0;
  ray.TMax = 1.0e30;

  Payload payload;
  payload.pointIdx = point_idx;
  payload.bestEid = 0xFFFFFFFFu;
  payload.bestT = 3.402823466e+38f;  // FLT_MAX
  payload._pad0 = 0u;

  TraceRay(gScene, RAY_FLAG_NONE, 0xFF,
           0,  // rayContributionToHitGroupIndex
           1,  // multiplierForGeometryContributionToHitGroupIndex
           0,  // missShaderIndex
           ray, payload);

  gClosestEids[point_idx] = payload.bestEid;
}

// -----------------------------------------------------------------------------
// Miss
// -----------------------------------------------------------------------------

[shader("miss")]
void missMain(inout Payload payload) {
  uint old1;
  InterlockedAdd(gDebugCounter[1], 1, old1);  // miss count
}

// -----------------------------------------------------------------------------
// Intersection
// -----------------------------------------------------------------------------
[shader("intersection")]
void intersectionMain() {
  uint prim_idx = PrimitiveIndex();

  uint3 ray_idx = DispatchRaysIndex();
  uint3 ray_dim = DispatchRaysDimensions();
  uint point_idx = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

  LaunchParamsPIP params = gParamsBuf[0];

  uint old2;
  InterlockedAdd(gDebugCounter[2], 1, old2);  // intersection invocations
  gDebugCounter[5] = prim_idx;                // last primitive

  PointD src_p = gQueryPoints[point_idx];
  double x_src_p = src_p.x;
  double y_src_p = src_p.y;

  EidRange range = gEidRanges[prim_idx];

  bool found = false;
  uint best_eid = 0xFFFFFFFFu;
  double best_y = 0.0;
  double best_t = 0.0;

  for (uint eid = range.first; eid < range.second; ++eid) {
    EdgeD e = gBaseEdges[eid];
    PointD p1 = gBasePoints[uint(e.p1_idx)];
    PointD p2 = gBasePoints[uint(e.p2_idx)];

    uint old3;
    InterlockedAdd(gDebugCounter[3], 1, old3);  // tested edges

    double x_min = min2_d(p1.x, p2.x);
    double x_max = max2_d(p1.x, p2.x);

    // Same simulation-of-simplicity rule as OptiX.
    if (x_src_p < x_min || x_src_p > x_max ||
        x_src_p == ((params.query_map_id == 0) ? x_min : x_max)) {
      continue;
    }

    // Vertical-line case should be excluded by the x-bound check / SoS rule.
    // Keep the guard anyway.
    if (e.b == 0.0) {
      continue;
    }

    double xsect_y = (-e.a * x_src_p - e.c) / e.b;
    double diff_y = y_src_p - xsect_y;

    if (diff_y == 0.0)
      diff_y = (params.query_map_id == 0) ? -e.a : e.a;
    if (diff_y == 0.0)
      diff_y = (params.query_map_id == 0) ? -e.b : e.b;
    if (diff_y == 0.0)
      continue;

    // Query point is above the candidate edge.
    if (diff_y > 0.0)
      continue;

    double candidate_t = xsect_y - y_src_p;
    if (candidate_t < 0.0)
      continue;

    if (!found) {
      found = true;
      best_eid = eid;
      best_y = xsect_y;
      best_t = candidate_t;
      continue;
    }

    if (xsect_y < best_y) {
      best_eid = eid;
      best_y = xsect_y;
      best_t = candidate_t;
      continue;
    }

    if (xsect_y == best_y) {
      EdgeD best_e = gBaseEdges[best_eid];
      if (preferCandidateAtSameT(params.query_map_id, e, best_e)) {
        best_eid = eid;
        best_y = xsect_y;
        best_t = candidate_t;
      }
    }
  }

  if (found) {
    HitAttr attr;
    attr.baseEid = best_eid;
    attr.candidateT = float(best_t);

    uint old6;
    InterlockedAdd(gDebugCounter[6], 1, old6);  // reported hits

    ReportHit(attr.candidateT, 0, attr);
  }
}

// -----------------------------------------------------------------------------
// Any-hit
// -----------------------------------------------------------------------------

[shader("anyhit")]
void anyHitMain(inout Payload payload, in HitAttr attr) {
  LaunchParamsPIP params = gParamsBuf[0];

  bool take = false;

  if (payload.bestEid == 0xFFFFFFFFu) {
    take = true;
  } else if (attr.candidateT < payload.bestT) {
    take = true;
  } else if (attr.candidateT == payload.bestT) {
    EdgeD candidate_e = gBaseEdges[attr.baseEid];
    EdgeD best_e = gBaseEdges[payload.bestEid];
    take = preferCandidateAtSameT(params.query_map_id, candidate_e, best_e);
  }

  if (take) {
    payload.bestEid = attr.baseEid;
    payload.bestT = attr.candidateT;

    uint old7;
    InterlockedAdd(gDebugCounter[7], 1, old7);  // accepted updates
  }

  // Keep traversing. We use payload as the reduction state and never commit a hit.
  IgnoreHit();
}

// -----------------------------------------------------------------------------
// Closest-hit
// -----------------------------------------------------------------------------

[shader("closesthit")]
void closestHitMain(inout Payload payload, in HitAttr attr) {
  // Intentionally empty.
  // Because anyHit always IgnoreHit(), closest-hit should normally not run.
  uint old8;
  InterlockedAdd(gDebugCounter[7], 1, old8);  // accepted updates
}
