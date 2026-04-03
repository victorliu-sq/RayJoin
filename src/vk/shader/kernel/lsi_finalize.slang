// import lsi_common;

// struct LaunchParamsLSIFinalize {
//   int query_map_id;
//   uint query_edge_count;
//   uint xsect_capacity;
//   uint pad0;
// };

// [[vk::binding(0, 0)]]
// StructuredBuffer<Edge> gBaseEdges;

// [[vk::binding(1, 0)]]
// StructuredBuffer<Point> gBasePoints;

// [[vk::binding(2, 0)]]
// StructuredBuffer<Edge> gQueryEdges;

// [[vk::binding(3, 0)]]
// StructuredBuffer<Point> gQueryPoints;

// [[vk::binding(4, 0)]]
// RWStructuredBuffer<Intersection128> gXsects;

// [[vk::binding(5, 0)]]
// StructuredBuffer<uint> gXsectCounter;

// [[vk::push_constant]]
// ConstantBuffer<LaunchParamsLSIFinalize> gParams;

// Rational128 make_rational_signed(Int128 num, Int128 den) {
//   Rational128 r;

//   if (i128_is_zero(den)) {
//     r.num = num;
//     r.den = den;
//     return r;
//   }

//   if (i128_is_neg(den)) {
//     num = i128_neg(num);
//     den = i128_neg(den);
//   }

//   r.num = num;
//   r.den = den;
//   return r;
// }

// bool rational_lt_int(Rational128 r, int64_t v) {
//   return i128_lt(r.num, mul_i128_i64(r.den, v));
// }

// bool rational_gt_int(Rational128 r, int64_t v) {
//   return i128_gt(r.num, mul_i128_i64(r.den, v));
// }

// [numthreads(64, 1, 1)]
// [shader("compute")]
// void main(uint3 tid : SV_DispatchThreadID) {
//   uint idx = tid.x;
//   uint nXsects = gXsectCounter[0];

//   if (idx >= gParams.xsect_capacity) return;
//   if (idx >= nXsects) return;

//   Intersection128 xsect = gXsects[idx];

//   uint query_map_id = (uint)gParams.query_map_id;
//   uint base_map_id = 1u - query_map_id;

//   uint base_eid  = (base_map_id == 0u) ? xsect.eid0 : xsect.eid1;
//   uint query_eid = (query_map_id == 0u) ? xsect.eid0 : xsect.eid1;

//   Edge base_e = gBaseEdges[base_eid];
//   Point base_p1 = gBasePoints[base_e.p1_idx];
//   Point base_p2 = gBasePoints[base_e.p2_idx];

//   Edge query_e = gQueryEdges[query_eid];
//   Point query_p1 = gQueryPoints[query_e.p1_idx];
//   Point query_p2 = gQueryPoints[query_e.p2_idx];

//   int64_t base_a = i128_to_i64_lossless(base_e.a);
//   int64_t base_b = i128_to_i64_lossless(base_e.b);
//   int64_t query_a = i128_to_i64_lossless(query_e.a);
//   int64_t query_b = i128_to_i64_lossless(query_e.b);

//   Int128 denom =
//       i128_sub(mul_i64_i64(base_a, query_b),
//                mul_i64_i64(query_a, base_b));

//   Int128 numx =
//       i128_sub(mul_i128_i64(query_e.c, base_b),
//                mul_i128_i64(base_e.c, query_b));

//   Int128 numy =
//       i128_sub(mul_i128_i64(base_e.c, query_a),
//                mul_i128_i64(query_e.c, base_a));

//   Rational128 rx = make_rational_signed(numx, denom);
//   Rational128 ry = make_rational_signed(numy, denom);

//   int64_t t;

//   t = min(min(base_p1.x, base_p2.x), min(query_p1.x, query_p2.x));
//   if (rational_lt_int(rx, t)) {
//     rx.num = i128_from_i64(t);
//     rx.den = i128_from_i64(1);
//   }

//   t = max(max(base_p1.x, base_p2.x), max(query_p1.x, query_p2.x));
//   if (rational_gt_int(rx, t)) {
//     rx.num = i128_from_i64(t);
//     rx.den = i128_from_i64(1);
//   }

//   t = min(min(base_p1.y, base_p2.y), min(query_p1.y, query_p2.y));
//   if (rational_lt_int(ry, t)) {
//     ry.num = i128_from_i64(t);
//     ry.den = i128_from_i64(1);
//   }

//   t = max(max(base_p1.y, base_p2.y), max(query_p1.y, query_p2.y));
//   if (rational_gt_int(ry, t)) {
//     ry.num = i128_from_i64(t);
//     ry.den = i128_from_i64(1);
//   }

//   xsect.x = rx;
//   xsect.y = ry;
//   gXsects[idx] = xsect;
// }

import lsi_common;

struct LaunchParamsLSIFinalize {
  int query_map_id;
  uint query_edge_count;
  uint xsect_capacity;
  uint pad0;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Edge> gBaseEdges;

[[vk::binding(1, 0)]]
StructuredBuffer<Point> gBasePoints;

[[vk::binding(2, 0)]]
StructuredBuffer<Edge> gQueryEdges;

[[vk::binding(3, 0)]]
StructuredBuffer<Point> gQueryPoints;

[[vk::binding(4, 0)]]
RWStructuredBuffer<Intersection128> gXsects;

[[vk::binding(5, 0)]]
StructuredBuffer<uint> gXsectCounter;

[[vk::push_constant]]
ConstantBuffer<LaunchParamsLSIFinalize> gParams;

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 tid: SV_DispatchThreadID) {
  uint idx = tid.x;
  uint nXsects = gXsectCounter[0];

  if (idx >= gParams.xsect_capacity)
    return;
  if (idx >= nXsects)
    return;

  Intersection128 xsect = gXsects[idx];

  uint query_map_id = (uint)gParams.query_map_id;
  uint base_map_id = 1u - query_map_id;

  uint base_eid = (base_map_id == 0u) ? xsect.eid0 : xsect.eid1;
  uint query_eid = (query_map_id == 0u) ? xsect.eid0 : xsect.eid1;

  Edge base_e = gBaseEdges[base_eid];
  Point base_p1 = gBasePoints[base_e.p1_idx];
  Point base_p2 = gBasePoints[base_e.p2_idx];

  Edge query_e = gQueryEdges[query_eid];
  Point query_p1 = gQueryPoints[query_e.p1_idx];
  Point query_p2 = gQueryPoints[query_e.p2_idx];

  // Match C++ exactly:
  // denom = (coefficient_t) base_e.a * query_e.b - (coefficient_t) query_e.a * base_e.b;
  // numx  = (coefficient_t) query_e.c * base_e.b - (coefficient_t) base_e.c * query_e.b;
  // numy  = (coefficient_t) query_e.a * base_e.c - (coefficient_t) base_e.a * query_e.c;

  Int128 denom = i128_sub(mul_i128_i128(base_e.a, query_e.b), mul_i128_i128(query_e.a, base_e.b));

  Int128 numx = i128_sub(mul_i128_i128(query_e.c, base_e.b), mul_i128_i128(base_e.c, query_e.b));

  Int128 numy = i128_sub(mul_i128_i128(query_e.a, base_e.c), mul_i128_i128(base_e.a, query_e.c));

  Rational128 rx = rational128_make(numx, denom);
  Rational128 ry = rational128_make(numy, denom);

  // Some other stuff
  // int64_t t;

  // t = min(min(base_p1.x, base_p2.x), min(query_p1.x, query_p2.x));
  // if (rational128_lt_i64(rx, t)) {
  //   rx = rational128_make(i128_from_i64(t), i128_from_i64(1));
  // }

  // t = max(max(base_p1.x, base_p2.x), max(query_p1.x, query_p2.x));
  // if (rational128_gt_i64(rx, t)) {
  //   rx = rational128_make(i128_from_i64(t), i128_from_i64(1));
  // }

  // t = min(min(base_p1.y, base_p2.y), min(query_p1.y, query_p2.y));
  // if (rational128_lt_i64(ry, t)) {
  //   ry = rational128_make(i128_from_i64(t), i128_from_i64(1));
  // }

  // t = max(max(base_p1.y, base_p2.y), max(query_p1.y, query_p2.y));
  // if (rational128_gt_i64(ry, t)) {
  //   ry = rational128_make(i128_from_i64(t), i128_from_i64(1));
  // }

  xsect.x = rx;
  xsect.y = ry;
  gXsects[idx] = xsect;
}
