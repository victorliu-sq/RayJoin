// module lsi_rt_ns_d64;

// // ============================================================================
// // NS RT shader
// // - Point coordinates are already double world-space values.
// // - Edge coefficients are double.
// // - No scaling buffer is used.
// // - Output intersections store x/y directly as double.
// // ============================================================================

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

// struct Intersection {
//   double x;
//   double y;

//   uint64_t eid0;
//   uint64_t eid1;

//   uint mid_point_polygon_id;
//   uint pad;
// };

// struct LaunchParamsLSI {
//   int query_map_id;
//   uint query_edge_count;
//   uint xsect_capacity;
//   uint _pad0;
// };

// // --------------------------------------------------------------------------
// // Bindings
// // --------------------------------------------------------------------------
// [[vk::binding(0, 0)]]
// RaytracingAccelerationStructure gScene;

// [[vk::binding(1, 0)]]
// StructuredBuffer<LaunchParamsLSI> gParamsBuf;

// [[vk::binding(2, 0)]]
// StructuredBuffer<EdgeD> gBaseEdges;

// [[vk::binding(3, 0)]]
// StructuredBuffer<PointD> gBasePoints;

// [[vk::binding(4, 0)]]
// StructuredBuffer<EidRange> gEidRanges;

// [[vk::binding(5, 0)]]
// StructuredBuffer<EdgeD> gQueryEdges;

// [[vk::binding(6, 0)]]
// StructuredBuffer<PointD> gQueryPoints;

// [[vk::binding(7, 0)]]
// RWStructuredBuffer<Intersection> gXsects;

// [[vk::binding(8, 0)]]
// RWStructuredBuffer<uint> gXsectCounter;

// [[vk::binding(9, 0)]]
// RWStructuredBuffer<uint> gTestCounter;

// // --------------------------------------------------------------------------
// // Payload
// // --------------------------------------------------------------------------
// struct Payload {
//   uint queryEid;
// };

// // --------------------------------------------------------------------------
// // Helpers
// // --------------------------------------------------------------------------
// bool pointEquals(PointD a, PointD b) {
//   return a.x == b.x && a.y == b.y;
// }

// double min2_d(double a, double b) {
//   return (a < b) ? a : b;
// }

// double max2_d(double a, double b) {
//   return (a > b) ? a : b;
// }

// double min4_d(double a, double b, double c, double d) {
//   return min2_d(min2_d(a, b), min2_d(c, d));
// }

// double max4_d(double a, double b, double c, double d) {
//   return max2_d(max2_d(a, b), max2_d(c, d));
// }

// double subedge(PointD p, EdgeD e) {
//   return p.x * e.a + p.y * e.b + e.c;
// }

// // Tie-break helpers, same role as the old integer version.
// double tieBreakNeg(EdgeD e) {
//   if (e.a != 0.0)
//     return -e.a;
//   if (e.b != 0.0)
//     return -e.b;
//   return 0.0;
// }

// double tieBreakPos(EdgeD e) {
//   if (e.a != 0.0)
//     return e.a;
//   if (e.b != 0.0)
//     return e.b;
//   return 0.0;
// }

// // -----------------------------------------------------------------------------
// // Boolean intersection predicate
// // -----------------------------------------------------------------------------
// bool intersect_test(EdgeD e1, PointD e1_p1, PointD e1_p2, EdgeD e2, PointD e2_p1, PointD e2_p2) {
//   double e2_p1_agst_e1 = subedge(e2_p1, e1);
//   double e2_p2_agst_e1 = subedge(e2_p2, e1);
//   double e1_p1_agst_e2 = subedge(e1_p1, e2);
//   double e1_p2_agst_e2 = subedge(e1_p2, e2);

//   if (e1_p1_agst_e2 == 0.0)
//     e1_p1_agst_e2 = tieBreakNeg(e2);
//   if (e1_p1_agst_e2 == 0.0)
//     return false;

//   if (e1_p2_agst_e2 == 0.0)
//     e1_p2_agst_e2 = tieBreakNeg(e2);
//   if (e1_p2_agst_e2 == 0.0)
//     return false;

//   if ((e1_p1_agst_e2 > 0.0 && e1_p2_agst_e2 > 0.0) ||
//       (e1_p1_agst_e2 < 0.0 && e1_p2_agst_e2 < 0.0)) {
//     return false;
//   }

//   if (e2_p1_agst_e1 == 0.0)
//     e2_p1_agst_e1 = tieBreakPos(e1);
//   if (e2_p1_agst_e1 == 0.0)
//     return false;

//   if (e2_p2_agst_e1 == 0.0)
//     e2_p2_agst_e1 = tieBreakPos(e1);
//   if (e2_p2_agst_e1 == 0.0)
//     return false;

//   if ((e2_p1_agst_e1 > 0.0 && e2_p2_agst_e1 > 0.0) ||
//       (e2_p1_agst_e1 < 0.0 && e2_p2_agst_e1 < 0.0)) {
//     return false;
//   }

//   if ((pointEquals(e1_p1, e2_p1) && pointEquals(e1_p2, e2_p2)) ||
//       (pointEquals(e1_p1, e2_p2) && pointEquals(e1_p2, e2_p1))) {
//     return false;
//   }

//   return true;
// }

// // -----------------------------------------------------------------------------
// // Intersection with point output
// // -----------------------------------------------------------------------------
// bool intersect_test_with_point(EdgeD e1, PointD e1_p1, PointD e1_p2, EdgeD e2, PointD e2_p1,
//                                PointD e2_p2, out double xsect_x, out double xsect_y) {
//   xsect_x = 0.0;
//   xsect_y = 0.0;

//   if (!intersect_test(e1, e1_p1, e1_p2, e2, e2_p1, e2_p2))
//     return false;

//   double denom = e1.a * e2.b - e2.a * e1.b;
//   if (denom == 0.0)
//     return false;

//   xsect_x = (e2.c * e1.b - e1.c * e2.b) / denom;
//   xsect_y = (e2.a * e1.c - e1.a * e2.c) / denom;

//   double t;

//   t = min4_d(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
//   if (xsect_x < t)
//     xsect_x = t;

//   t = max4_d(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
//   if (xsect_x > t)
//     xsect_x = t;

//   t = min4_d(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
//   if (xsect_y < t)
//     xsect_y = t;

//   t = max4_d(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
//   if (xsect_y > t)
//     xsect_y = t;

//   return true;
// }

// // ============================================================================
// // Ray generation
// // ============================================================================
// [shader("raygeneration")]
// void raygenMain() {
//   uint eid = DispatchRaysIndex().x;

//   uint old0;
//   InterlockedAdd(gTestCounter[0], 1, old0);

//   gTestCounter[4] = eid;

//   LaunchParamsLSI params = gParamsBuf[0];
//   if (eid >= params.query_edge_count)
//     return;

//   EdgeD e = gQueryEdges[eid];
//   PointD p1 = gQueryPoints[uint(e.p1_idx)];
//   PointD p2 = gQueryPoints[uint(e.p2_idx)];

//   // Match previous ordering policy:
//   // vertical edge -> order by y
//   // otherwise     -> order by x
//   if (e.b == 0.0) {
//     if (p1.y > p2.y) {
//       PointD tmp = p1;
//       p1 = p2;
//       p2 = tmp;
//     }
//   } else {
//     if (p1.x > p2.x) {
//       PointD tmp = p1;
//       p1 = p2;
//       p2 = tmp;
//     }
//   }

//   RayDesc ray;
//   ray.Origin = float3(float(p1.x), float(p1.y), 0.0);
//   ray.Direction = float3(float(p2.x - p1.x), float(p2.y - p1.y), 0.0);
//   ray.TMin = 0.0;
//   ray.TMax = 1.0;

//   Payload payload;
//   payload.queryEid = eid;

//   TraceRay(gScene, RAY_FLAG_NONE, 0xFF,
//            0,  // rayContributionToHitGroupIndex
//            1,  // multiplierForGeometryContributionToHitGroupIndex
//            0,  // missShaderIndex
//            ray, payload);
// }

// // ============================================================================
// // Miss
// // ============================================================================
// [shader("miss")]
// void missMain(inout Payload payload) {
//   uint old1;
//   InterlockedAdd(gTestCounter[1], 1, old1);
// }

// // ============================================================================
// // Intersection
// // ============================================================================
// [shader("intersection")]
// void intersectionMain() {
//   uint prim_idx = PrimitiveIndex();

//   uint3 ray_idx = DispatchRaysIndex();
//   uint3 ray_dim = DispatchRaysDimensions();
//   uint ray_id = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

//   uint query_eid = ray_id;

//   uint old2;
//   InterlockedAdd(gTestCounter[2], 1, old2);
//   gTestCounter[5] = prim_idx;

//   LaunchParamsLSI params = gParamsBuf[0];
//   uint query_map_id = (uint)params.query_map_id;

//   EdgeD query_e = gQueryEdges[query_eid];
//   PointD query_e_p1 = gQueryPoints[uint(query_e.p1_idx)];
//   PointD query_e_p2 = gQueryPoints[uint(query_e.p2_idx)];

//   EidRange range = gEidRanges[prim_idx];
//   uint begin_eid = range.first;
//   uint end_eid = range.second;

//   for (uint base_eid = begin_eid; base_eid < end_eid; ++base_eid) {
//     EdgeD base_e = gBaseEdges[base_eid];
//     PointD base_e_p1 = gBasePoints[uint(base_e.p1_idx)];
//     PointD base_e_p2 = gBasePoints[uint(base_e.p2_idx)];

//     uint old3;
//     InterlockedAdd(gTestCounter[3], 1, old3);

//     double xq, yq;
//     if (intersect_test_with_point(query_e, query_e_p1, query_e_p2, base_e, base_e_p1, base_e_p2,
//     xq,
//                                   yq)) {
//       uint outIdx;
//       InterlockedAdd(gXsectCounter[0], 1, outIdx);

//       if (outIdx < params.xsect_capacity) {
//         Intersection x;
//         x.x = xq;
//         x.y = yq;

//         if (query_map_id == 0) {
//           x.eid0 = uint64_t(query_eid);
//           x.eid1 = uint64_t(base_eid);
//         } else {
//           x.eid0 = uint64_t(base_eid);
//           x.eid1 = uint64_t(query_eid);
//         }

//         x.mid_point_polygon_id = 0xFFFFFFFFu;
//         x.pad = 0u;

//         gXsects[outIdx] = x;
//       }
//     }
//   }

//   uint nTests = end_eid - begin_eid;
//   uint old6;
//   InterlockedAdd(gTestCounter[6], nTests, old6);
// }

// // ============================================================================
// // Any-hit / closest-hit
// // ============================================================================
// [shader("anyhit")]
// void anyHitMain(inout Payload payload) {
//   // Intentionally empty.
// }

// [shader("closesthit")]
// void closestHitMain(inout Payload payload) {
//   // Intentionally empty.
// }

// module lsi_rt_ns_d64;

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

// struct Intersection {
//   double x;
//   double y;

//   uint64_t eid0;
//   uint64_t eid1;

//   uint mid_point_polygon_id;
//   uint pad;
// };

// struct LaunchParamsLSI {
//   int query_map_id;
//   uint query_edge_count;
//   uint xsect_capacity;
//   uint _pad0;
// };

// [[vk::binding(0, 0)]]
// RaytracingAccelerationStructure gScene;

// [[vk::push_constant]]
// ConstantBuffer<LaunchParamsLSI> gParams;

// [[vk::binding(1, 0)]]
// StructuredBuffer<EdgeD> gBaseEdges;

// [[vk::binding(2, 0)]]
// StructuredBuffer<PointD> gBasePoints;

// [[vk::binding(3, 0)]]
// StructuredBuffer<EidRange> gEidRanges;

// [[vk::binding(4, 0)]]
// StructuredBuffer<EdgeD> gQueryEdges;

// [[vk::binding(5, 0)]]
// StructuredBuffer<PointD> gQueryPoints;

// [[vk::binding(6, 0)]]
// RWStructuredBuffer<Intersection> gXsects;

// [[vk::binding(7, 0)]]
// RWStructuredBuffer<uint> gXsectCounter;

// [[vk::binding(8, 0)]]
// RWStructuredBuffer<uint> gTestCounter;

// struct Payload {
//   uint queryEid;
// };

// bool pointEquals(PointD a, PointD b) {
//   return a.x == b.x && a.y == b.y;
// }

// double min2_d(double a, double b) {
//   return (a < b) ? a : b;
// }

// double max2_d(double a, double b) {
//   return (a > b) ? a : b;
// }

// double min4_d(double a, double b, double c, double d) {
//   return min2_d(min2_d(a, b), min2_d(c, d));
// }

// double max4_d(double a, double b, double c, double d) {
//   return max2_d(max2_d(a, b), max2_d(c, d));
// }

// double subedge(PointD p, EdgeD e) {
//   return p.x * e.a + p.y * e.b + e.c;
// }

// double tieBreakNeg(EdgeD e) {
//   if (e.a != 0.0)
//     return -e.a;
//   if (e.b != 0.0)
//     return -e.b;
//   return 0.0;
// }

// double tieBreakPos(EdgeD e) {
//   if (e.a != 0.0)
//     return e.a;
//   if (e.b != 0.0)
//     return e.b;
//   return 0.0;
// }

// bool intersect_test(EdgeD e1, PointD e1_p1, PointD e1_p2, EdgeD e2, PointD e2_p1, PointD e2_p2) {
//   double e2_p1_agst_e1 = subedge(e2_p1, e1);
//   double e2_p2_agst_e1 = subedge(e2_p2, e1);
//   double e1_p1_agst_e2 = subedge(e1_p1, e2);
//   double e1_p2_agst_e2 = subedge(e1_p2, e2);

//   if (e1_p1_agst_e2 == 0.0)
//     e1_p1_agst_e2 = tieBreakNeg(e2);
//   if (e1_p1_agst_e2 == 0.0)
//     return false;

//   if (e1_p2_agst_e2 == 0.0)
//     e1_p2_agst_e2 = tieBreakNeg(e2);
//   if (e1_p2_agst_e2 == 0.0)
//     return false;

//   if ((e1_p1_agst_e2 > 0.0 && e1_p2_agst_e2 > 0.0) ||
//       (e1_p1_agst_e2 < 0.0 && e1_p2_agst_e2 < 0.0)) {
//     return false;
//   }

//   if (e2_p1_agst_e1 == 0.0)
//     e2_p1_agst_e1 = tieBreakPos(e1);
//   if (e2_p1_agst_e1 == 0.0)
//     return false;

//   if (e2_p2_agst_e1 == 0.0)
//     e2_p2_agst_e1 = tieBreakPos(e1);
//   if (e2_p2_agst_e1 == 0.0)
//     return false;

//   if ((e2_p1_agst_e1 > 0.0 && e2_p2_agst_e1 > 0.0) ||
//       (e2_p1_agst_e1 < 0.0 && e2_p2_agst_e1 < 0.0)) {
//     return false;
//   }

//   if ((pointEquals(e1_p1, e2_p1) && pointEquals(e1_p2, e2_p2)) ||
//       (pointEquals(e1_p1, e2_p2) && pointEquals(e1_p2, e2_p1))) {
//     return false;
//   }

//   return true;
// }

// bool intersect_test_with_point(EdgeD e1, PointD e1_p1, PointD e1_p2, EdgeD e2, PointD e2_p1,
//                                PointD e2_p2, out double xsect_x, out double xsect_y) {
//   xsect_x = 0.0;
//   xsect_y = 0.0;

//   if (!intersect_test(e1, e1_p1, e1_p2, e2, e2_p1, e2_p2))
//     return false;

//   double denom = e1.a * e2.b - e2.a * e1.b;
//   if (denom == 0.0)
//     return false;

//   xsect_x = (e2.c * e1.b - e1.c * e2.b) / denom;
//   xsect_y = (e2.a * e1.c - e1.a * e2.c) / denom;

//   double t;

//   t = min4_d(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
//   if (xsect_x < t)
//     xsect_x = t;

//   t = max4_d(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
//   if (xsect_x > t)
//     xsect_x = t;

//   t = min4_d(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
//   if (xsect_y < t)
//     xsect_y = t;

//   t = max4_d(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
//   if (xsect_y > t)
//     xsect_y = t;

//   return true;
// }

// [shader("raygeneration")]
// void raygenMain() {
//   uint eid = DispatchRaysIndex().x;

//   uint old0;
//   InterlockedAdd(gTestCounter[0], 1, old0);

//   gTestCounter[4] = eid;

//   LaunchParamsLSI params = gParams;
//   if (eid >= params.query_edge_count)
//     return;

//   EdgeD e = gQueryEdges[eid];
//   PointD p1 = gQueryPoints[uint(e.p1_idx)];
//   PointD p2 = gQueryPoints[uint(e.p2_idx)];

//   if (e.b == 0.0) {
//     if (p1.y > p2.y) {
//       PointD tmp = p1;
//       p1 = p2;
//       p2 = tmp;
//     }
//   } else {
//     if (p1.x > p2.x) {
//       PointD tmp = p1;
//       p1 = p2;
//       p2 = tmp;
//     }
//   }

//   RayDesc ray;
//   ray.Origin = float3(float(p1.x), float(p1.y), 0.0);
//   ray.Direction = float3(float(p2.x - p1.x), float(p2.y - p1.y), 0.0);
//   ray.TMin = 0.0;
//   ray.TMax = 1.0;

//   Payload payload;
//   payload.queryEid = eid;

//   TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
// }

// [shader("miss")]
// void missMain(inout Payload payload) {
//   uint old1;
//   InterlockedAdd(gTestCounter[1], 1, old1);
// }

// [shader("intersection")]
// void intersectionMain() {
//   uint prim_idx = PrimitiveIndex();

//   uint3 ray_idx = DispatchRaysIndex();
//   uint3 ray_dim = DispatchRaysDimensions();
//   uint ray_id = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

//   uint query_eid = ray_id;

//   uint old2;
//   InterlockedAdd(gTestCounter[2], 1, old2);
//   gTestCounter[5] = prim_idx;

//   LaunchParamsLSI params = gParams;
//   uint query_map_id = (uint)params.query_map_id;

//   EdgeD query_e = gQueryEdges[query_eid];
//   PointD query_e_p1 = gQueryPoints[uint(query_e.p1_idx)];
//   PointD query_e_p2 = gQueryPoints[uint(query_e.p2_idx)];

//   EidRange range = gEidRanges[prim_idx];
//   uint begin_eid = range.first;
//   uint end_eid = range.second;

//   for (uint base_eid = begin_eid; base_eid < end_eid; ++base_eid) {
//     EdgeD base_e = gBaseEdges[base_eid];
//     PointD base_e_p1 = gBasePoints[uint(base_e.p1_idx)];
//     PointD base_e_p2 = gBasePoints[uint(base_e.p2_idx)];

//     uint old3;
//     InterlockedAdd(gTestCounter[3], 1, old3);

//     double xq, yq;
//     if (intersect_test_with_point(query_e, query_e_p1, query_e_p2, base_e, base_e_p1, base_e_p2,
//     xq,
//                                   yq)) {
//       uint outIdx;
//       InterlockedAdd(gXsectCounter[0], 1, outIdx);

//       if (outIdx < params.xsect_capacity) {
//         Intersection x;
//         x.x = xq;
//         x.y = yq;

//         if (query_map_id == 0) {
//           x.eid0 = uint64_t(query_eid);
//           x.eid1 = uint64_t(base_eid);
//         } else {
//           x.eid0 = uint64_t(base_eid);
//           x.eid1 = uint64_t(query_eid);
//         }

//         x.mid_point_polygon_id = 0xFFFFFFFFu;
//         x.pad = 0u;

//         gXsects[outIdx] = x;
//       }
//     }
//   }

//   uint nTests = end_eid - begin_eid;
//   uint old6;
//   InterlockedAdd(gTestCounter[6], nTests, old6);
// }

// [shader("anyhit")]
// void anyHitMain(inout Payload payload) {}

// [shader("closesthit")]
// void closestHitMain(inout Payload payload) {}

module lsi_rt_ns_d64;

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

struct Intersection {
  double x;
  double y;

  uint eid0;
  uint eid1;

  int mid_point_polygon_id;
  int pad;
};

struct LaunchParamsLSI {
  int query_map_id;
  uint query_edge_count;
  uint xsect_capacity;
  uint _pad0;
};

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure gScene;

[[vk::push_constant]]
ConstantBuffer<LaunchParamsLSI> gParams;

[[vk::binding(1, 0)]]
StructuredBuffer<EdgeD> gBaseEdges;

[[vk::binding(2, 0)]]
StructuredBuffer<PointD> gBasePoints;

[[vk::binding(3, 0)]]
StructuredBuffer<EidRange> gEidRanges;

[[vk::binding(4, 0)]]
StructuredBuffer<EdgeD> gQueryEdges;

[[vk::binding(5, 0)]]
StructuredBuffer<PointD> gQueryPoints;

[[vk::binding(6, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::binding(7, 0)]]
RWStructuredBuffer<uint> gXsectCounter;

[[vk::binding(8, 0)]]
RWStructuredBuffer<uint> gTestCounter;

struct Payload {
  uint queryEid;
};

bool pointEquals(PointD a, PointD b) {
  return a.x == b.x && a.y == b.y;
}

double min2_d(double a, double b) {
  return (a < b) ? a : b;
}

double max2_d(double a, double b) {
  return (a > b) ? a : b;
}

double min4_d(double a, double b, double c, double d) {
  return min2_d(min2_d(a, b), min2_d(c, d));
}

double max4_d(double a, double b, double c, double d) {
  return max2_d(max2_d(a, b), max2_d(c, d));
}

double subedge(PointD p, EdgeD e) {
  return p.x * e.a + p.y * e.b + e.c;
}

double tieBreakNeg(EdgeD e) {
  if (e.a != 0.0)
    return -e.a;
  if (e.b != 0.0)
    return -e.b;
  return 0.0;
}

double tieBreakPos(EdgeD e) {
  if (e.a != 0.0)
    return e.a;
  if (e.b != 0.0)
    return e.b;
  return 0.0;
}

bool intersect_test(EdgeD e1, PointD e1_p1, PointD e1_p2, EdgeD e2, PointD e2_p1, PointD e2_p2) {
  double e2_p1_agst_e1 = subedge(e2_p1, e1);
  double e2_p2_agst_e1 = subedge(e2_p2, e1);
  double e1_p1_agst_e2 = subedge(e1_p1, e2);
  double e1_p2_agst_e2 = subedge(e1_p2, e2);

  if (e1_p1_agst_e2 == 0.0)
    e1_p1_agst_e2 = tieBreakNeg(e2);
  if (e1_p1_agst_e2 == 0.0)
    return false;

  if (e1_p2_agst_e2 == 0.0)
    e1_p2_agst_e2 = tieBreakNeg(e2);
  if (e1_p2_agst_e2 == 0.0)
    return false;

  if ((e1_p1_agst_e2 > 0.0 && e1_p2_agst_e2 > 0.0) ||
      (e1_p1_agst_e2 < 0.0 && e1_p2_agst_e2 < 0.0)) {
    return false;
  }

  if (e2_p1_agst_e1 == 0.0)
    e2_p1_agst_e1 = tieBreakPos(e1);
  if (e2_p1_agst_e1 == 0.0)
    return false;

  if (e2_p2_agst_e1 == 0.0)
    e2_p2_agst_e1 = tieBreakPos(e1);
  if (e2_p2_agst_e1 == 0.0)
    return false;

  if ((e2_p1_agst_e1 > 0.0 && e2_p2_agst_e1 > 0.0) ||
      (e2_p1_agst_e1 < 0.0 && e2_p2_agst_e1 < 0.0)) {
    return false;
  }

  if ((pointEquals(e1_p1, e2_p1) && pointEquals(e1_p2, e2_p2)) ||
      (pointEquals(e1_p1, e2_p2) && pointEquals(e1_p2, e2_p1))) {
    return false;
  }

  return true;
}

bool intersect_test_with_point(EdgeD e1, PointD e1_p1, PointD e1_p2, EdgeD e2, PointD e2_p1,
                               PointD e2_p2, out double xsect_x, out double xsect_y) {
  xsect_x = 0.0;
  xsect_y = 0.0;

  if (!intersect_test(e1, e1_p1, e1_p2, e2, e2_p1, e2_p2))
    return false;

  double denom = e1.a * e2.b - e2.a * e1.b;
  if (denom == 0.0)
    return false;

  xsect_x = (e2.c * e1.b - e1.c * e2.b) / denom;
  xsect_y = (e2.a * e1.c - e1.a * e2.c) / denom;

  double t;

  t = min4_d(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
  if (xsect_x < t)
    xsect_x = t;

  t = max4_d(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
  if (xsect_x > t)
    xsect_x = t;

  t = min4_d(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
  if (xsect_y < t)
    xsect_y = t;

  t = max4_d(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
  if (xsect_y > t)
    xsect_y = t;

  return true;
}

[shader("raygeneration")]
void raygenMain() {
  uint eid = DispatchRaysIndex().x;

  uint old0;
  InterlockedAdd(gTestCounter[0], 1, old0);

  gTestCounter[4] = eid;

  LaunchParamsLSI params = gParams;
  if (eid >= params.query_edge_count)
    return;

  EdgeD e = gQueryEdges[eid];
  PointD p1 = gQueryPoints[e.p1_idx];
  PointD p2 = gQueryPoints[e.p2_idx];

  if (e.b == 0.0) {
    if (p1.y > p2.y) {
      PointD tmp = p1;
      p1 = p2;
      p2 = tmp;
    }
  } else {
    if (p1.x > p2.x) {
      PointD tmp = p1;
      p1 = p2;
      p2 = tmp;
    }
  }

  RayDesc ray;
  ray.Origin = float3(float(p1.x), float(p1.y), 0.0);
  ray.Direction = float3(float(p2.x - p1.x), float(p2.y - p1.y), 0.0);
  ray.TMin = 0.0;
  ray.TMax = 1.0;

  Payload payload;
  payload.queryEid = eid;

  TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
}

[shader("miss")]
void missMain(inout Payload payload) {
  uint old1;
  InterlockedAdd(gTestCounter[1], 1, old1);
}

[shader("intersection")]
void intersectionMain() {
  uint prim_idx = PrimitiveIndex();

  uint3 ray_idx = DispatchRaysIndex();
  uint3 ray_dim = DispatchRaysDimensions();
  uint ray_id = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

  uint query_eid = ray_id;

  uint old2;
  InterlockedAdd(gTestCounter[2], 1, old2);
  gTestCounter[5] = prim_idx;

  LaunchParamsLSI params = gParams;
  uint query_map_id = (uint)params.query_map_id;

  EdgeD query_e = gQueryEdges[query_eid];
  PointD query_e_p1 = gQueryPoints[query_e.p1_idx];
  PointD query_e_p2 = gQueryPoints[query_e.p2_idx];

  EidRange range = gEidRanges[prim_idx];
  uint begin_eid = range.first;
  uint end_eid = range.second;

  for (uint base_eid = begin_eid; base_eid < end_eid; ++base_eid) {
    EdgeD base_e = gBaseEdges[base_eid];
    PointD base_e_p1 = gBasePoints[base_e.p1_idx];
    PointD base_e_p2 = gBasePoints[base_e.p2_idx];

    uint old3;
    InterlockedAdd(gTestCounter[3], 1, old3);

    double xq, yq;
    if (intersect_test_with_point(query_e, query_e_p1, query_e_p2, base_e, base_e_p1, base_e_p2, xq,
                                  yq)) {
      uint outIdx;
      InterlockedAdd(gXsectCounter[0], 1, outIdx);

      if (outIdx < params.xsect_capacity) {
        Intersection x;
        x.x = xq;
        x.y = yq;

        if (query_map_id == 0) {
          x.eid0 = query_eid;
          x.eid1 = base_eid;
        } else {
          x.eid0 = base_eid;
          x.eid1 = query_eid;
        }

        x.mid_point_polygon_id = -1;
        x.pad = 0;

        gXsects[outIdx] = x;
      }
    }
  }

  uint nTests = end_eid - begin_eid;
  uint old6;
  InterlockedAdd(gTestCounter[6], nTests, old6);
}

[shader("anyhit")]
void anyHitMain(inout Payload payload) {}

[shader("closesthit")]
void closestHitMain(inout Payload payload) {}
