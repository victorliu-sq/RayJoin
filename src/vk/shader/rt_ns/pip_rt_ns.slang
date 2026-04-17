/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// With AnyHit without slope
// module pip_rt_ns_d64;

// struct PointD {
//   double x;
//   double y;
// };

// struct EdgeD {
//   double a;
//   double b;
//   double c;

//   uint eid;
//   uint p1_idx;
//   uint p2_idx;
//   uint left_polygon_id;
//   uint right_polygon_id;
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

// [[vk::push_constant]]
// ConstantBuffer<LaunchParamsPIP> gParams;

// [[vk::binding(0, 0)]]
// RaytracingAccelerationStructure gScene;

// [[vk::binding(1, 0)]]
// StructuredBuffer<EdgeD> gBaseEdges;

// [[vk::binding(2, 0)]]
// StructuredBuffer<PointD> gBasePoints;

// [[vk::binding(3, 0)]]
// StructuredBuffer<EidRange> gEidRanges;

// [[vk::binding(4, 0)]]
// StructuredBuffer<PointD> gQueryPoints;

// [[vk::binding(5, 0)]]
// RWStructuredBuffer<uint> gClosestEids;

// [[vk::binding(6, 0)]]
// RWStructuredBuffer<double> gBestYs;

// [[vk::binding(7, 0)]]
// RWStructuredBuffer<uint> gDebugCounter;

// struct Payload {
//   uint pointIdx;
//   uint bestEid;
//   uint bestYLow;
//   uint bestYHigh;
// };

// struct HitAttr {
//   uint baseEid;
//   uint yLo;
//   uint yHi;
// };

// double min2_d(double a, double b) {
//   return (a < b) ? a : b;
// }

// double max2_d(double a, double b) {
//   return (a > b) ? a : b;
// }

// void packDoubleToU32(double v, out uint lo, out uint hi) {
//   asuint(v, lo, hi);
// }

// double unpackDoubleFromU32(uint lo, uint hi) {
//   return asdouble(lo, hi);
// }

// bool preferCandidateAtSameY(uint candidate_eid, uint best_eid) {
//   return candidate_eid < best_eid;
// }

// bool betterCandidate(uint candidate_eid, double candidate_y, uint best_eid, double best_y) {
//   if (candidate_y < best_y) {
//     return true;
//   }
//   if (candidate_y > best_y) {
//     return false;
//   }

//   return preferCandidateAtSameY(candidate_eid, best_eid);
// }

// bool computeCandidateY(uint query_map_id, PointD src_p, EdgeD e, out double xsect_y) {
//   PointD p1 = gBasePoints[e.p1_idx];
//   PointD p2 = gBasePoints[e.p2_idx];

//   double x_min = min2_d(p1.x, p2.x);
//   double x_max = max2_d(p1.x, p2.x);

//   if (src_p.x < x_min || src_p.x > x_max || src_p.x == ((query_map_id == 0) ? x_min : x_max)) {
//     xsect_y = 0.0;
//     return false;
//   }

//   if (e.b == 0.0) {
//     xsect_y = 0.0;
//     return false;
//   }

//   xsect_y = (-e.a * src_p.x - e.c) / e.b;
//   double diff_y = src_p.y - xsect_y;

//   if (diff_y == 0.0)
//     diff_y = (query_map_id == 0) ? -e.a : e.a;
//   if (diff_y == 0.0)
//     diff_y = (query_map_id == 0) ? -e.b : e.b;
//   if (diff_y == 0.0)
//     return false;

//   if (diff_y > 0.0)
//     return false;

//   return true;
// }

// [shader("raygeneration")]
// void raygenMain() {
//   uint point_idx = DispatchRaysIndex().x;
//   LaunchParamsPIP params = gParams;

//   if (point_idx >= params.query_point_count)
//     return;

//   PointD p = gQueryPoints[point_idx];

//   RayDesc ray;
//   ray.Origin = float3(float(p.x), float(p.y), 0.0);
//   ray.Direction = float3(0.0, 1.0, 0.0);
//   ray.TMin = 0.0;
//   ray.TMax = 1.0e30;

//   Payload payload;
//   payload.pointIdx = point_idx;
//   payload.bestEid = 0xFFFFFFFFu;
//   packDoubleToU32(1.0e300, payload.bestYLow, payload.bestYHigh);

//   TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

//   gClosestEids[point_idx] = payload.bestEid;
//   gBestYs[point_idx] = unpackDoubleFromU32(payload.bestYLow, payload.bestYHigh);
// }

// [shader("miss")]
// void missMain(inout Payload payload) {}

// [shader("intersection")]
// void intersectionMain() {
//   uint prim_idx = PrimitiveIndex();

//   uint3 ray_idx = DispatchRaysIndex();
//   uint3 ray_dim = DispatchRaysDimensions();
//   uint point_idx = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

//   LaunchParamsPIP params = gParams;
//   PointD src_p = gQueryPoints[point_idx];
//   EidRange range = gEidRanges[prim_idx];

//   bool found = false;
//   uint local_best_eid = 0xFFFFFFFFu;
//   double local_best_y = 0.0;
//   float local_best_t = 0.0f;

//   for (uint eid = range.first; eid < range.second; ++eid) {
//     EdgeD e = gBaseEdges[eid];

//     double xsect_y = 0.0;
//     if (!computeCandidateY(uint(params.query_map_id), src_p, e, xsect_y)) {
//       continue;
//     }

//     if (!found || betterCandidate(eid, xsect_y, local_best_eid, local_best_y)) {
//       found = true;
//       local_best_eid = eid;
//       local_best_y = xsect_y;
//       local_best_t = float(xsect_y - src_p.y);
//     }
//   }

//   if (!found) {
//     return;
//   }

//   HitAttr attr;
//   attr.baseEid = local_best_eid;
//   packDoubleToU32(local_best_y, attr.yLo, attr.yHi);

//   ReportHit(local_best_t, 0, attr);
// }

// [shader("anyhit")]
// void anyHitMain(inout Payload payload, in HitAttr attr) {
//   uint candidate_eid = attr.baseEid;
//   double candidate_y = unpackDoubleFromU32(attr.yLo, attr.yHi);

//   bool take = false;

//   if (payload.bestEid == 0xFFFFFFFFu) {
//     take = true;
//   } else {
//     double best_y = unpackDoubleFromU32(payload.bestYLow, payload.bestYHigh);

//     if (candidate_y < best_y) {
//       take = true;
//     } else if (candidate_y == best_y) {
//       take = preferCandidateAtSameY(candidate_eid, payload.bestEid);
//     }
//   }

//   if (take) {
//     payload.bestEid = candidate_eid;
//     payload.bestYLow = attr.yLo;
//     payload.bestYHigh = attr.yHi;
//   }

//   IgnoreHit();
// }

// [shader("closesthit")]
// void closestHitMain(inout Payload payload, in HitAttr attr) {}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// Without AnyHit without slope
module pip_rt_ns_d64;

struct PointD {
  double x;
  double y;
};

struct EdgeD {
  double a;
  double b;
  double c;

  uint eid;
  uint p1_idx;
  uint p2_idx;
  uint left_polygon_id;
  uint right_polygon_id;
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

[[vk::push_constant]]
ConstantBuffer<LaunchParamsPIP> gParams;

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure gScene;

[[vk::binding(1, 0)]]
StructuredBuffer<EdgeD> gBaseEdges;

[[vk::binding(2, 0)]]
StructuredBuffer<PointD> gBasePoints;

[[vk::binding(3, 0)]]
StructuredBuffer<EidRange> gEidRanges;

[[vk::binding(4, 0)]]
StructuredBuffer<PointD> gQueryPoints;

[[vk::binding(5, 0)]]
RWStructuredBuffer<uint> gClosestEids;

[[vk::binding(6, 0)]]
RWStructuredBuffer<double> gBestYs;

[[vk::binding(7, 0)]]
RWStructuredBuffer<uint> gDebugCounter;

struct Payload {
  uint pointIdx;
  uint bestEid;
  uint bestYLow;
  uint bestYHigh;
};

struct HitAttr {
  uint baseEid;
  uint yLo;
  uint yHi;
};

double min2_d(double a, double b) {
  return (a < b) ? a : b;
}

double max2_d(double a, double b) {
  return (a > b) ? a : b;
}

void packDoubleToU32(double v, out uint lo, out uint hi) {
  asuint(v, lo, hi);
}

double unpackDoubleFromU32(uint lo, uint hi) {
  return asdouble(lo, hi);
}

bool preferCandidateAtSameY(uint candidate_eid, uint best_eid) {
  return candidate_eid < best_eid;
}

bool betterCandidate(uint candidate_eid, double candidate_y, uint best_eid, double best_y) {
  if (candidate_y < best_y) {
    return true;
  }
  if (candidate_y > best_y) {
    return false;
  }

  return preferCandidateAtSameY(candidate_eid, best_eid);
}

bool computeCandidateY(uint query_map_id, PointD src_p, EdgeD e, out double xsect_y) {
  PointD p1 = gBasePoints[e.p1_idx];
  PointD p2 = gBasePoints[e.p2_idx];

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

  if (diff_y > 0.0)
    return false;

  return true;
}

[shader("raygeneration")]
void raygenMain() {
  uint point_idx = DispatchRaysIndex().x;
  LaunchParamsPIP params = gParams;

  if (point_idx >= params.query_point_count)
    return;

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

  // Initialize outputs so miss rays still leave sentinel values.
  gClosestEids[point_idx] = 0xFFFFFFFFu;
  gBestYs[point_idx] = 1.0e300;

  TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
}

[shader("miss")]
void missMain(inout Payload payload) {}

[shader("intersection")]
void intersectionMain() {
  uint prim_idx = PrimitiveIndex();

  uint3 ray_idx = DispatchRaysIndex();
  uint3 ray_dim = DispatchRaysDimensions();
  uint point_idx = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

  LaunchParamsPIP params = gParams;
  PointD src_p = gQueryPoints[point_idx];
  EidRange range = gEidRanges[prim_idx];

  bool found = false;
  uint local_best_eid = 0xFFFFFFFFu;
  double local_best_y = 0.0;
  float local_best_t = 0.0f;

  for (uint eid = range.first; eid < range.second; ++eid) {
    EdgeD e = gBaseEdges[eid];

    double xsect_y = 0.0;
    if (!computeCandidateY(uint(params.query_map_id), src_p, e, xsect_y)) {
      continue;
    }

    if (!found || betterCandidate(eid, xsect_y, local_best_eid, local_best_y)) {
      found = true;
      local_best_eid = eid;
      local_best_y = xsect_y;
      local_best_t = float(xsect_y - src_p.y);
    }
  }

  if (!found) {
    return;
  }

  double best_y = gBestYs[point_idx];
  uint best_eid = gClosestEids[point_idx];

  if (best_eid == 0xFFFFFFFFu || betterCandidate(local_best_eid, local_best_y, best_eid, best_y)) {
    gBestYs[point_idx] = local_best_y;
    gClosestEids[point_idx] = local_best_eid;
  }

  HitAttr attr;
  attr.baseEid = local_best_eid;
  packDoubleToU32(local_best_y, attr.yLo, attr.yHi);

  ReportHit(local_best_t, 0, attr);
}

[shader("anyhit")]
void anyHitMain(inout Payload payload, in HitAttr attr) {}

[shader("closesthit")]
void closestHitMain(inout Payload payload, in HitAttr attr) {}
