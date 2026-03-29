// module pip_rt_ns_d64;

// struct PointD {
//   double x;
//   double y;
// };

// struct EdgeD {
//   double a;
//   double b;
//   double c;

//   uint64_t eid;
//   uint64_t p1_idx;
//   uint64_t p2_idx;
//   uint64_t left_polygon_id;
//   uint64_t right_polygon_id;
// };

// struct EidRange {
//   uint first;
//   uint second;
// };

// struct LaunchParamsPIP {
//   int query_map_id;
//   uint query_point_count;
//   uint _pad0;
//   uint _pad1;
// };

// [[vk::binding(0, 0)]]
// RaytracingAccelerationStructure gScene;

// [[vk::binding(1, 0)]]
// StructuredBuffer<LaunchParamsPIP> gParamsBuf;

// [[vk::binding(2, 0)]]
// StructuredBuffer<EdgeD> gBaseEdges;

// [[vk::binding(3, 0)]]
// StructuredBuffer<PointD> gBasePoints;

// [[vk::binding(4, 0)]]
// StructuredBuffer<EidRange> gEidRanges;

// [[vk::binding(5, 0)]]
// StructuredBuffer<PointD> gQueryPoints;

// [[vk::binding(6, 0)]]
// RWStructuredBuffer<uint> gClosestEids;

// [[vk::binding(7, 0)]]
// RWStructuredBuffer<uint> gDebugCounter;

// // -----------------------------------------------------------------------------
// // Payload and hit attributes
// // -----------------------------------------------------------------------------

// struct Payload {
//   uint pointIdx;
//   uint bestEid;
//   // float bestT;
//   // uint _pad0;

//   uint bestTLow;
//   uint bestTHigh;
// };

// struct HitAttr {
//   uint baseEid;
//   // float candidateT;

//   uint candidateTLow;
//   uint candidateTHigh;
// };

// // -----------------------------------------------------------------------------
// // Helpers
// // -----------------------------------------------------------------------------

// double min2_d(double a, double b) {
//   return (a < b) ? a : b;
// }

// double max2_d(double a, double b) {
//   return (a > b) ? a : b;
// }

// double edgeSlope(EdgeD e) {
//   // Same quantity used by the OptiX code
//   return e.a / e.b;
// }

// // Returns whether candidate_e should replace best_e when both have same t.
// // This mirrors the OptiX tie-break policy.
// // bool preferCandidateAtSameT(uint query_map_id, EdgeD candidate_e, EdgeD best_e) {
// //   double candidate_slope = edgeSlope(candidate_e);
// //   double best_slope = edgeSlope(best_e);
// //   bool flag = candidate_slope > best_slope;

// //   // OptiX:
// //   // if ((query_map_id && !flag) || (flag && !query_map_id)) continue;
// //   // meaning candidate is preferred iff NOT that condition.
// //   return !((query_map_id != 0 && !flag) || (flag && query_map_id == 0));
// // }

// bool preferCandidateAtSameT(uint query_map_id, EdgeD candidate_e, EdgeD best_e) {
//   double candidate_slope = edgeSlope(candidate_e);
//   double best_slope = edgeSlope(best_e);

//   if (candidate_slope != best_slope) {
//     bool flag = candidate_slope > best_slope;

//     // OptiX:
//     // candidate is preferred iff NOT:
//     //   (query_map_id && !flag) || (flag && !query_map_id)
//     return !((query_map_id != 0 && !flag) || (flag && query_map_id == 0));
//   }

//   // Final deterministic tie-break: smaller eid wins.
//   return candidate_e.eid < best_e.eid;
// }

// void packDoubleToU32(double v, out uint lo, out uint hi) {
//   asuint(v, lo, hi);
// }

// double unpackDoubleFromU32(uint lo, uint hi) {
//   return asdouble(lo, hi);
// }

// // -----------------------------------------------------------------------------
// // Ray generation
// // -----------------------------------------------------------------------------

// [shader("raygeneration")]
// void raygenMain() {
//   uint point_idx = DispatchRaysIndex().x;
//   LaunchParamsPIP params = gParamsBuf[0];

//   if (point_idx >= params.query_point_count)
//     return;

//   uint old0;
//   InterlockedAdd(gDebugCounter[0], 1, old0);  // raygen count
//   gDebugCounter[4] = point_idx;               // last point

//   PointD p = gQueryPoints[point_idx];

//   RayDesc ray;
//   ray.Origin = float3(float(p.x), float(p.y), 0.0);
//   ray.Direction = float3(0.0, 1.0, 0.0);
//   ray.TMin = 0.0;
//   ray.TMax = 1.0e30;

//   Payload payload;
//   payload.pointIdx = point_idx;
//   payload.bestEid = 0xFFFFFFFFu;

//   // payload.bestT = 3.402823466e+38f;  // FLT_MAX
//   // payload._pad0 = 0u;

//   packDoubleToU32(1.0e300, payload.bestTLow, payload.bestTHigh);

//   TraceRay(gScene, RAY_FLAG_NONE, 0xFF,
//            0,  // rayContributionToHitGroupIndex
//            1,  // multiplierForGeometryContributionToHitGroupIndex
//            0,  // missShaderIndex
//            ray, payload);

//   gClosestEids[point_idx] = payload.bestEid;
// }

// // -----------------------------------------------------------------------------
// // Miss
// // -----------------------------------------------------------------------------

// [shader("miss")]
// void missMain(inout Payload payload) {
//   uint old1;
//   InterlockedAdd(gDebugCounter[1], 1, old1);  // miss count
// }

// // -----------------------------------------------------------------------------
// // Intersection
// // -----------------------------------------------------------------------------
// [shader("intersection")]
// void intersectionMain() {
//   uint prim_idx = PrimitiveIndex();

//   uint3 ray_idx = DispatchRaysIndex();
//   uint3 ray_dim = DispatchRaysDimensions();
//   uint point_idx = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

//   LaunchParamsPIP params = gParamsBuf[0];

//   uint old2;
//   InterlockedAdd(gDebugCounter[2], 1, old2);  // intersection invocations
//   gDebugCounter[5] = prim_idx;                // last primitive

//   PointD src_p = gQueryPoints[point_idx];
//   double x_src_p = src_p.x;
//   double y_src_p = src_p.y;

//   EidRange range = gEidRanges[prim_idx];

//   bool found = false;
//   uint best_eid = 0xFFFFFFFFu;
//   double best_y = 0.0;
//   double best_t = 0.0;

//   for (uint eid = range.first; eid < range.second; ++eid) {
//     EdgeD e = gBaseEdges[eid];
//     PointD p1 = gBasePoints[uint(e.p1_idx)];
//     PointD p2 = gBasePoints[uint(e.p2_idx)];

//     uint old3;
//     InterlockedAdd(gDebugCounter[3], 1, old3);  // tested edges

//     double x_min = min2_d(p1.x, p2.x);
//     double x_max = max2_d(p1.x, p2.x);

//     // Same simulation-of-simplicity rule as OptiX.
//     if (x_src_p < x_min || x_src_p > x_max ||
//         x_src_p == ((params.query_map_id == 0) ? x_min : x_max)) {
//       continue;
//     }

//     // Vertical-line case should be excluded by the x-bound check / SoS rule.
//     // Keep the guard anyway.
//     if (e.b == 0.0) {
//       continue;
//     }

//     double xsect_y = (-e.a * x_src_p - e.c) / e.b;
//     double diff_y = y_src_p - xsect_y;

//     if (diff_y == 0.0)
//       diff_y = (params.query_map_id == 0) ? -e.a : e.a;
//     if (diff_y == 0.0)
//       diff_y = (params.query_map_id == 0) ? -e.b : e.b;
//     if (diff_y == 0.0)
//       continue;

//     // Query point is above the candidate edge.
//     if (diff_y > 0.0)
//       continue;

//     double candidate_t = xsect_y - y_src_p;
//     if (candidate_t < 0.0)
//       continue;

//     if (!found) {
//       found = true;
//       best_eid = eid;
//       best_y = xsect_y;
//       best_t = candidate_t;
//       continue;
//     }

//     if (xsect_y < best_y) {
//       best_eid = eid;
//       best_y = xsect_y;
//       best_t = candidate_t;
//       continue;
//     }

//     if (xsect_y == best_y) {
//       EdgeD best_e = gBaseEdges[best_eid];
//       if (preferCandidateAtSameT(params.query_map_id, e, best_e)) {
//         best_eid = eid;
//         best_y = xsect_y;
//         best_t = candidate_t;
//       }
//     }
//   }

//   if (found) {
//     HitAttr attr;
//     attr.baseEid = best_eid;
//     // attr.candidateT = float(best_t);
//     packDoubleToU32(best_t, attr.candidateTLow, attr.candidateTHigh);

//     uint old6;
//     InterlockedAdd(gDebugCounter[6], 1, old6);  // reported hits

//     // ReportHit(attr.candidateT, 0, attr);
//     ReportHit(float(best_t), 0, attr);
//   }
// }

// // -----------------------------------------------------------------------------
// // Any-hit
// // -----------------------------------------------------------------------------

// [shader("anyhit")]
// void anyHitMain(inout Payload payload, in HitAttr attr) {
//   LaunchParamsPIP params = gParamsBuf[0];

//   bool take = false;

//   if (payload.bestEid == 0xFFFFFFFFu) {
//     take = true;
//   } else {
//     double candidate_t = unpackDoubleFromU32(attr.candidateTLow, attr.candidateTHigh);
//     double best_t = unpackDoubleFromU32(payload.bestTLow, payload.bestTHigh);

//     if (candidate_t < best_t) {
//       take = true;
//     } else if (candidate_t == best_t) {
//       EdgeD candidate_e = gBaseEdges[attr.baseEid];
//       EdgeD best_e = gBaseEdges[payload.bestEid];
//       take = preferCandidateAtSameT(uint(params.query_map_id), candidate_e, best_e);
//     }
//   }

//   if (take) {
//     payload.bestEid = attr.baseEid;
//     // payload.bestT = attr.candidateT;
//     payload.bestTLow = attr.candidateTLow;
//     payload.bestTHigh = attr.candidateTHigh;

//     uint old7;
//     InterlockedAdd(gDebugCounter[7], 1, old7);  // accepted updates
//   }

//   // Keep traversing. We use payload as the reduction state and never commit a hit.
//   IgnoreHit();
// }

// // -----------------------------------------------------------------------------
// // Closest-hit
// // -----------------------------------------------------------------------------

// [shader("closesthit")]
// void closestHitMain(inout Payload payload, in HitAttr attr) {
//   // Intentionally empty.
//   // Because anyHit always IgnoreHit(), closest-hit should normally not run.
//   uint old8;
//   InterlockedAdd(gDebugCounter[7], 1, old8);  // accepted updates
// }

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
RWStructuredBuffer<double> gBestYs;

[[vk::binding(8, 0)]]
RWStructuredBuffer<uint> gDebugCounter;

// -----------------------------------------------------------------------------
// Payload and hit attributes
// -----------------------------------------------------------------------------

struct Payload {
  uint pointIdx;
  uint bestEid;

  uint bestYLow;
  uint bestYHigh;
};

struct HitAttr {
  uint baseEid;
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static const double kTieEps = 1.0e-15;

double min2_d(double a, double b) {
  return (a < b) ? a : b;
}

double max2_d(double a, double b) {
  return (a > b) ? a : b;
}

double edgeSlope(EdgeD e) {
  return e.a / e.b;
}

bool nearlyEqualD(double a, double b) {
  return abs(a - b) <= kTieEps;
}

bool definitelyLessD(double a, double b) {
  return a < (b - kTieEps);
}

bool definitelyGreaterD(double a, double b) {
  return a > (b + kTieEps);
}

void packDoubleToU32(double v, out uint lo, out uint hi) {
  asuint(v, lo, hi);
}

double unpackDoubleFromU32(uint lo, uint hi) {
  return asdouble(lo, hi);
}

bool preferCandidateAtSameY(uint query_map_id, EdgeD candidate_e, EdgeD best_e) {
  double candidate_slope = edgeSlope(candidate_e);
  double best_slope = edgeSlope(best_e);

  if (!nearlyEqualD(candidate_slope, best_slope)) {
    bool flag = candidate_slope > best_slope;

    // Match OptiX policy:
    // if im==0 -> prefer bigger slope
    // if im==1 -> prefer smaller slope
    return !((query_map_id != 0 && !flag) || (flag && query_map_id == 0));
  }

  // Final deterministic tie-break: smaller eid wins.
  return candidate_e.eid < best_e.eid;
}

// Recompute the same candidate y used by OptiX-style logic.
bool computeCandidateY(uint query_map_id, PointD src_p, EdgeD e, out double xsect_y) {
  PointD p1 = gBasePoints[uint(e.p1_idx)];
  PointD p2 = gBasePoints[uint(e.p2_idx)];

  double x_min = min2_d(p1.x, p2.x);
  double x_max = max2_d(p1.x, p2.x);

  if (src_p.x < x_min || src_p.x > x_max || src_p.x == ((query_map_id == 0) ? x_min : x_max)) {
    xsect_y = 0.0;
    return false;
  }

  if (e.b == 0.0) {
    xsect_y = 0.0;
    return false;
  }

  xsect_y = (-e.a * src_p.x - e.c) / e.b;
  double diff_y = src_p.y - xsect_y;

  if (diff_y == 0.0)
    diff_y = (query_map_id == 0) ? -e.a : e.a;
  if (diff_y == 0.0)
    diff_y = (query_map_id == 0) ? -e.b : e.b;
  if (diff_y == 0.0)
    return false;

  // current point is above the current edge
  if (diff_y > 0.0)
    return false;

  return true;
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
  InterlockedAdd(gDebugCounter[0], 1, old0);
  gDebugCounter[4] = point_idx;

  PointD p = gQueryPoints[point_idx];

  RayDesc ray;
  ray.Origin = float3(float(p.x), float(p.y), 0.0);
  ray.Direction = float3(0.0, 1.0, 0.0);
  ray.TMin = 0.0;
  ray.TMax = 1.0e30;

  Payload payload;
  payload.pointIdx = point_idx;
  payload.bestEid = 0xFFFFFFFFu;
  packDoubleToU32(1.0e300, payload.bestYLow, payload.bestYHigh);

  TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

  gClosestEids[point_idx] = payload.bestEid;
  gBestYs[point_idx] = unpackDoubleFromU32(payload.bestYLow, payload.bestYHigh);
}

// -----------------------------------------------------------------------------
// Miss
// -----------------------------------------------------------------------------

[shader("miss")]
void missMain(inout Payload payload) {
  uint old1;
  InterlockedAdd(gDebugCounter[1], 1, old1);
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
  InterlockedAdd(gDebugCounter[2], 1, old2);
  gDebugCounter[5] = prim_idx;

  PointD src_p = gQueryPoints[point_idx];
  EidRange range = gEidRanges[prim_idx];

  // Do not locally reduce to one edge here.
  // Report every valid candidate edge and let anyHit do the global reduction.
  for (uint eid = range.first; eid < range.second; ++eid) {
    EdgeD e = gBaseEdges[eid];

    uint old3;
    InterlockedAdd(gDebugCounter[3], 1, old3);

    double xsect_y = 0.0;
    if (!computeCandidateY(uint(params.query_map_id), src_p, e, xsect_y)) {
      continue;
    }

    HitAttr attr;
    attr.baseEid = eid;

    uint old6;
    InterlockedAdd(gDebugCounter[6], 1, old6);

    // Constant hit t; reduction is done in anyHit via payload.bestY.
    ReportHit(0.0, 0, attr);
  }
}

// -----------------------------------------------------------------------------
// Any-hit
// -----------------------------------------------------------------------------

[shader("anyhit")]
void anyHitMain(inout Payload payload, in HitAttr attr) {
  LaunchParamsPIP params = gParamsBuf[0];
  PointD src_p = gQueryPoints[payload.pointIdx];

  EdgeD candidate_e = gBaseEdges[attr.baseEid];
  double candidate_y = 0.0;

  if (!computeCandidateY(uint(params.query_map_id), src_p, candidate_e, candidate_y)) {
    IgnoreHit();
    return;
  }

  bool take = false;

  if (payload.bestEid == 0xFFFFFFFFu) {
    take = true;
  } else {
    double best_y = unpackDoubleFromU32(payload.bestYLow, payload.bestYHigh);
    EdgeD best_e = gBaseEdges[payload.bestEid];

    if (definitelyLessD(candidate_y, best_y)) {
      take = true;
    } else if (nearlyEqualD(candidate_y, best_y)) {
      take = preferCandidateAtSameY(uint(params.query_map_id), candidate_e, best_e);
    }
  }

  if (take) {
    payload.bestEid = attr.baseEid;
    packDoubleToU32(candidate_y, payload.bestYLow, payload.bestYHigh);

    uint old7;
    InterlockedAdd(gDebugCounter[7], 1, old7);
  }

  IgnoreHit();
}

// -----------------------------------------------------------------------------
// Closest-hit
// -----------------------------------------------------------------------------

[shader("closesthit")]
void closestHitMain(inout Payload payload, in HitAttr attr) {
  uint old8;
  InterlockedAdd(gDebugCounter[7], 1, old8);
}
