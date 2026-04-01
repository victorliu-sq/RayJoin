import edge;

struct Point {
  int64_t x;
  int64_t y;
};

struct Rational64 {
  int64_t num;
  int64_t den;
};

struct Intersection {
  Rational64 x;
  Rational64 y;

  uint64_t eid0;
  uint64_t eid1;

  uint mid_point_polygon_id;
  uint pad;
};

struct LaunchParamsLSI {
  int query_map_id;
  uint query_edge_count;
  uint xsect_capacity;
  uint _pad0;
};

[[vk::binding(0, 0)]]
StructuredBuffer<LaunchParamsLSI> gParamsBuf;

[[vk::binding(1, 0)]]
StructuredBuffer<Edge> gBaseEdges;

[[vk::binding(2, 0)]]
StructuredBuffer<Point> gBasePoints;

[[vk::binding(3, 0)]]
StructuredBuffer<Edge> gQueryEdges;

[[vk::binding(4, 0)]]
StructuredBuffer<Point> gQueryPoints;

[[vk::binding(5, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::binding(6, 0)]]
StructuredBuffer<uint> gXsectCounter;

// -----------------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------------
int64_t abs_i64(int64_t v) {
  return (v < 0) ? -v : v;
}

int64_t min2_i64(int64_t a, int64_t b) {
  return (a < b) ? a : b;
}

int64_t max2_i64(int64_t a, int64_t b) {
  return (a > b) ? a : b;
}

int64_t min4_i64(int64_t a, int64_t b, int64_t c, int64_t d) {
  return min2_i64(min2_i64(a, b), min2_i64(c, d));
}

int64_t max4_i64(int64_t a, int64_t b, int64_t c, int64_t d) {
  return max2_i64(max2_i64(a, b), max2_i64(c, d));
}

int64_t gcd_i64(int64_t a, int64_t b) {
  a = abs_i64(a);
  b = abs_i64(b);

  if (a == 0) return (b == 0) ? 1 : b;
  if (b == 0) return a;

  while (b != 0) {
    int64_t t = a % b;
    a = b;
    b = t;
  }
  return (a == 0) ? 1 : a;
}

// Match tcb::rational behavior:
//   - reduce by gcd
//   - denominator always non-negative
Rational64 makeRational64Canonical(int64_t num, int64_t den) {
  Rational64 r;

  // Degenerate case should not normally happen for real crossings,
  // but keep it defined.
  if (den == 0) {
    r.num = num;
    r.den = 0;
    return r;
  }

  int64_t g = gcd_i64(num, den);
  num /= g;
  den /= g;

  if (den < 0) {
    num = -num;
    den = -den;
  }

  r.num = num;
  r.den = den;
  return r;
}

bool rationalLessThanInt(Rational64 r, int64_t v) {
  return r.num < v * r.den;
}

bool rationalGreaterThanInt(Rational64 r, int64_t v) {
  return r.num > v * r.den;
}

[numthreads(256, 1, 1)]
[shader("compute")]
void main(uint3 tid : SV_DispatchThreadID) {
  uint idx = tid.x;

  LaunchParamsLSI params = gParamsBuf[0];
  uint nXsects = gXsectCounter[0];

  if (idx >= params.xsect_capacity) return;
  if (idx >= nXsects) return;

  Intersection xsect = gXsects[idx];

  uint query_map_id = (uint)params.query_map_id;
  uint base_map_id = 1 - query_map_id;

  uint64_t base_eid  = (base_map_id  == 0) ? xsect.eid0 : xsect.eid1;
  uint64_t query_eid = (query_map_id == 0) ? xsect.eid0 : xsect.eid1;

  Edge base_e = gBaseEdges[uint(base_eid)];
  Point base_e_p1 = gBasePoints[uint(base_e.p1_idx)];
  Point base_e_p2 = gBasePoints[uint(base_e.p2_idx)];

  Edge query_e = gQueryEdges[uint(query_eid)];
  Point query_e_p1 = gQueryPoints[uint(query_e.p1_idx)];
  Point query_e_p2 = gQueryPoints[uint(query_e.p2_idx)];

  int64_t denom = int64_t(base_e.a) * int64_t(query_e.b) -
                  int64_t(query_e.a) * int64_t(base_e.b);

  int64_t numx = int64_t(query_e.c) * int64_t(base_e.b) -
                 int64_t(base_e.c) * int64_t(query_e.b);

  int64_t numy = int64_t(query_e.a) * int64_t(base_e.c) -
                 int64_t(base_e.a) * int64_t(query_e.c);

  Rational64 xsect_x = makeRational64Canonical(numx, denom);
  Rational64 xsect_y = makeRational64Canonical(numy, denom);

  int64_t t;

  t = min4_i64(base_e_p1.x, base_e_p2.x, query_e_p1.x, query_e_p2.x);
  if (rationalLessThanInt(xsect_x, t)) {
    xsect_x = makeRational64Canonical(t, 1);
  }

  t = max4_i64(base_e_p1.x, base_e_p2.x, query_e_p1.x, query_e_p2.x);
  if (rationalGreaterThanInt(xsect_x, t)) {
    xsect_x = makeRational64Canonical(t, 1);
  }

  t = min4_i64(base_e_p1.y, base_e_p2.y, query_e_p1.y, query_e_p2.y);
  if (rationalLessThanInt(xsect_y, t)) {
    xsect_y = makeRational64Canonical(t, 1);
  }

  t = max4_i64(base_e_p1.y, base_e_p2.y, query_e_p1.y, query_e_p2.y);
  if (rationalGreaterThanInt(xsect_y, t)) {
    xsect_y = makeRational64Canonical(t, 1);
  }

  xsect.x = xsect_x;
  xsect.y = xsect_y;

  // Leave these untouched for now, per your request:
  // xsect.mid_point_polygon_id
  // xsect.pad

  gXsects[idx] = xsect;
}