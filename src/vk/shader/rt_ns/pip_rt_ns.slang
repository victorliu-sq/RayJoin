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
// };

// double min2_d(double a, double b) {
//   return (a < b) ? a : b;
// }

// double max2_d(double a, double b) {
//   return (a > b) ? a : b;
// }

// double edgeSlope(EdgeD e) {
//   return e.a / e.b;
// }

// void packDoubleToU32(double v, out uint lo, out uint hi) {
//   asuint(v, lo, hi);
// }

// double unpackDoubleFromU32(uint lo, uint hi) {
//   return asdouble(lo, hi);
// }

// bool preferCandidateAtSameY(uint query_map_id, EdgeD candidate_e, EdgeD best_e) {
//   double candidate_slope = edgeSlope(candidate_e);
//   double best_slope = edgeSlope(best_e);

//   if (candidate_slope != best_slope) {
//     bool flag = candidate_slope > best_slope;
//     return !((query_map_id != 0 && !flag) || (flag && query_map_id == 0));
//   }

//   return candidate_e.eid < best_e.eid;
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

//   // uint old0;
//   // InterlockedAdd(gDebugCounter[0], 1, old0);
//   // gDebugCounter[4] = point_idx;

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
// void missMain(inout Payload payload) {
//   // uint old1;
//   // InterlockedAdd(gDebugCounter[1], 1, old1);
// }

// [shader("intersection")]
// void intersectionMain() {
//   uint prim_idx = PrimitiveIndex();

//   uint3 ray_idx = DispatchRaysIndex();
//   uint3 ray_dim = DispatchRaysDimensions();
//   uint point_idx = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

//   LaunchParamsPIP params = gParams;

//   // uint old2;
//   // InterlockedAdd(gDebugCounter[2], 1, old2);
//   // gDebugCounter[5] = prim_idx;

//   PointD src_p = gQueryPoints[point_idx];
//   EidRange range = gEidRanges[prim_idx];

//   for (uint eid = range.first; eid < range.second; ++eid) {
//     EdgeD e = gBaseEdges[eid];

//     // uint old3;
//     // InterlockedAdd(gDebugCounter[3], 1, old3);

//     double xsect_y = 0.0;
//     if (!computeCandidateY(uint(params.query_map_id), src_p, e, xsect_y)) {
//       continue;
//     }

//     HitAttr attr;
//     attr.baseEid = eid;

//     // uint old6;
//     // InterlockedAdd(gDebugCounter[6], 1, old6);

//     ReportHit(0.0, 0, attr);
//   }
// }

// [shader("anyhit")]
// void anyHitMain(inout Payload payload, in HitAttr attr) {
//   LaunchParamsPIP params = gParams;
//   PointD src_p = gQueryPoints[payload.pointIdx];

//   EdgeD candidate_e = gBaseEdges[attr.baseEid];
//   double candidate_y = 0.0;

//   if (!computeCandidateY(uint(params.query_map_id), src_p, candidate_e, candidate_y)) {
//     IgnoreHit();
//     return;
//   }

//   bool take = false;

//   if (payload.bestEid == 0xFFFFFFFFu) {
//     take = true;
//   } else {
//     double best_y = unpackDoubleFromU32(payload.bestYLow, payload.bestYHigh);
//     EdgeD best_e = gBaseEdges[payload.bestEid];

//     if (candidate_y < best_y) {
//       take = true;
//     } else if (candidate_y == best_y) {
//       take = preferCandidateAtSameY(uint(params.query_map_id), candidate_e, best_e);
//     }
//   }

//   if (take) {
//     payload.bestEid = attr.baseEid;
//     packDoubleToU32(candidate_y, payload.bestYLow, payload.bestYHigh);

//     // uint old7;
//     // InterlockedAdd(gDebugCounter[7], 1, old7);
//   }

//   IgnoreHit();
// }

// [shader("closesthit")]
// void closestHitMain(inout Payload payload, in HitAttr attr) {
//   // uint old8;
//   // InterlockedAdd(gDebugCounter[7], 1, old8);
// }

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
// };

// double min2_d(double a, double b) {
//   return (a < b) ? a : b;
// }

// double max2_d(double a, double b) {
//   return (a > b) ? a : b;
// }

// double edgeSlope(EdgeD e) {
//   return e.a / e.b;
// }

// void packDoubleToU32(double v, out uint lo, out uint hi) {
//   asuint(v, lo, hi);
// }

// double unpackDoubleFromU32(uint lo, uint hi) {
//   return asdouble(lo, hi);
// }

// bool preferCandidateAtSameY(uint query_map_id, EdgeD candidate_e, EdgeD best_e) {
//   double candidate_slope = edgeSlope(candidate_e);
//   double best_slope = edgeSlope(best_e);

//   if (candidate_slope != best_slope) {
//     bool flag = candidate_slope > best_slope;
//     return !((query_map_id != 0 && !flag) || (flag && query_map_id == 0));
//   }

//   return candidate_e.eid < best_e.eid;
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

//   // Initialize per-point outputs before traversal.
//   gClosestEids[point_idx] = 0xFFFFFFFFu;
//   gBestYs[point_idx] = 1.0e300;

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

//   for (uint eid = range.first; eid < range.second; ++eid) {
//     EdgeD e = gBaseEdges[eid];

//     double xsect_y = 0.0;
//     if (!computeCandidateY(uint(params.query_map_id), src_p, e, xsect_y)) {
//       continue;
//     }

//     if (!found) {
//       found = true;
//       local_best_eid = eid;
//       local_best_y = xsect_y;
//       continue;
//     }

//     if (xsect_y < local_best_y) {
//       local_best_eid = eid;
//       local_best_y = xsect_y;
//       continue;
//     }

//     if (xsect_y == local_best_y) {
//       EdgeD prev_best_e = gBaseEdges[local_best_eid];
//       if (preferCandidateAtSameY(uint(params.query_map_id), e, prev_best_e)) {
//         local_best_eid = eid;
//         local_best_y = xsect_y;
//       }
//     }
//   }

//   if (!found) {
//     return;
//   }

//   uint global_best_eid = gClosestEids[point_idx];
//   double global_best_y = gBestYs[point_idx];

//   bool take = false;
//   if (global_best_eid == 0xFFFFFFFFu) {
//     take = true;
//   } else if (local_best_y < global_best_y) {
//     take = true;
//   }
//   // else if (local_best_y == global_best_y) {
//   //   EdgeD candidate_e = gBaseEdges[local_best_eid];
//   //   EdgeD best_e = gBaseEdges[global_best_eid];
//   //   take = preferCandidateAtSameY(uint(params.query_map_id), candidate_e, best_e);
//   // }

//   if (take) {
//     gClosestEids[point_idx] = local_best_eid;
//     gBestYs[point_idx] = local_best_y;
//   }
// }

// // [shader("intersection")]
// // void intersectionMain() {
// //   uint prim_idx = PrimitiveIndex();
// //   uint3 ray_idx = DispatchRaysIndex();
// //   uint3 ray_dim = DispatchRaysDimensions();
// //   uint point_idx = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

// //   LaunchParamsPIP params = gParams;
// //   PointD src_p = gQueryPoints[point_idx];
// //   EidRange range = gEidRanges[prim_idx];

// //   // double best_y = gBestYs[point_idx];
// //   // uint best_e_eid = gClosestEids[point_idx];
// //   double best_y = 1e30;
// //   uint best_e_eid = 0xFFFFFFFFu;

// //   bool update_best = false;

// //   for (uint eid = range.first; eid < range.second; ++eid) {
// //     EdgeD e = gBaseEdges[eid];

// //     double xsect_y = 0.0;
// //     if (!computeCandidateY(uint(params.query_map_id), src_p, e, xsect_y)) {
// //       continue;
// //     }

// //     if (xsect_y > best_y) {
// //       continue;
// //     }

// //     if (xsect_y == best_y) {
// //       if (best_e_eid == 0xFFFFFFFFu) {
// //         // no previous best eid, so take this one
// //       } else {
// //         EdgeD best_e = gBaseEdges[best_e_eid];
// //         double current_e_slope = e.a / e.b;
// //         double best_e_slope = best_e.a / best_e.b;

// //         if (current_e_slope != best_e_slope) {
// //           bool flag = current_e_slope > best_e_slope;
// //           if ((params.query_map_id != 0 && !flag) || (flag && params.query_map_id == 0)) {
// //             continue;
// //           }
// //         } else {
// //           if (eid >= best_e_eid) {
// //             continue;
// //           }
// //         }
// //       }
// //     }

// //     best_y = xsect_y;
// //     best_e_eid = eid;
// //     update_best = true;
// //   }

// //   if (update_best) {
// //     // gBestYs[point_idx] = best_y;
// //     // gClosestEids[point_idx] = best_e_eid;
// //   }
// // }

// [shader("anyhit")]
// void anyHitMain(inout Payload payload, in HitAttr attr) {}

// [shader("closesthit")]
// void closestHitMain(inout Payload payload, in HitAttr attr) {}

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

  TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

  gClosestEids[point_idx] = payload.bestEid;
  gBestYs[point_idx] = unpackDoubleFromU32(payload.bestYLow, payload.bestYHigh);
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
  float best_t = 3.402823466e+38F;

  for (uint eid = range.first; eid < range.second; ++eid) {
    EdgeD e = gBaseEdges[eid];

    double xsect_y = 0.0;
    if (!computeCandidateY(uint(params.query_map_id), src_p, e, xsect_y)) {
      continue;
    }

    if (!found || xsect_y < local_best_y) {
      found = true;
      local_best_eid = eid;
      local_best_y = xsect_y;
      best_t = float(xsect_y - src_p.y);
    }
  }

  if (!found) {
    return;
  }

  // HitAttr attr;
  // attr.baseEid = local_best_eid;
  // packDoubleToU32(local_best_y, attr.yLo, attr.yHi);

  // ReportHit(best_t, 0, attr);

  HitAttr attr;
  attr.baseEid = local_best_eid;
  packDoubleToU32(local_best_y, attr.yLo, attr.yHi);

  ReportHit(best_t, 0, attr);
}

// [shader("anyhit")]
// void anyHitMain(inout Payload payload, in HitAttr attr) {}

[shader("anyhit")]
void anyHitMain(inout Payload payload, in HitAttr attr) {
  // double candidate_y = unpackDoubleFromU32(attr.yLo, attr.yHi);

  // bool take = false;

  // if (payload.bestEid == 0xFFFFFFFFu) {
  //   take = true;
  // } else {
  //   double best_y = unpackDoubleFromU32(payload.bestYLow, payload.bestYHigh);

  //   if (candidate_y < best_y) {
  //     take = true;
  //   }
  // }

  // if (take) {
  //   payload.bestEid = attr.baseEid;
  //   payload.bestYLow = attr.yLo;
  //   payload.bestYHigh = attr.yHi;
  // }

  // IgnoreHit(); // 10x perforamnce slowdown
}

[shader("closesthit")]
void closestHitMain(inout Payload payload, in HitAttr attr) {
  payload.bestEid = attr.baseEid;
  payload.bestYLow = attr.yLo;
  payload.bestYHigh = attr.yHi;
}
