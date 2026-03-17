module lsi_finalize_ns;

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

struct Intersection {
  double x;
  double y;

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
StructuredBuffer<EdgeD> gBaseEdges;

[[vk::binding(2, 0)]]
StructuredBuffer<PointD> gBasePoints;

[[vk::binding(3, 0)]]
StructuredBuffer<EdgeD> gQueryEdges;

[[vk::binding(4, 0)]]
StructuredBuffer<PointD> gQueryPoints;

[[vk::binding(5, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::binding(6, 0)]]
StructuredBuffer<uint> gXsectCounter;

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

[numthreads(256, 1, 1)]
[shader("compute")]
void main(uint3 tid: SV_DispatchThreadID) {
  uint idx = tid.x;

  LaunchParamsLSI params = gParamsBuf[0];
  uint nXsects = gXsectCounter[0];

  if (idx >= params.xsect_capacity)
    return;
  if (idx >= nXsects)
    return;

  Intersection xsect = gXsects[idx];

  uint query_map_id = (uint)params.query_map_id;
  uint base_map_id = 1 - query_map_id;

  uint64_t base_eid = (base_map_id == 0) ? xsect.eid0 : xsect.eid1;
  uint64_t query_eid = (query_map_id == 0) ? xsect.eid0 : xsect.eid1;

  EdgeD base_e = gBaseEdges[uint(base_eid)];
  PointD base_e_p1 = gBasePoints[uint(base_e.p1_idx)];
  PointD base_e_p2 = gBasePoints[uint(base_e.p2_idx)];

  EdgeD query_e = gQueryEdges[uint(query_eid)];
  PointD query_e_p1 = gQueryPoints[uint(query_e.p1_idx)];
  PointD query_e_p2 = gQueryPoints[uint(query_e.p2_idx)];

  double denom = base_e.a * query_e.b - query_e.a * base_e.b;

  // Parallel / degenerate safeguard.
  if (denom == 0.0) {
    // leave x/y unchanged
    gXsects[idx] = xsect;
    return;
  }

  double x = (query_e.c * base_e.b - base_e.c * query_e.b) / denom;
  double y = (query_e.a * base_e.c - base_e.a * query_e.c) / denom;

  // Clamp to bbox of both segments, same spirit as the old finalize pass.
  double t;

  t = min4_d(base_e_p1.x, base_e_p2.x, query_e_p1.x, query_e_p2.x);
  if (x < t)
    x = t;

  t = max4_d(base_e_p1.x, base_e_p2.x, query_e_p1.x, query_e_p2.x);
  if (x > t)
    x = t;

  t = min4_d(base_e_p1.y, base_e_p2.y, query_e_p1.y, query_e_p2.y);
  if (y < t)
    y = t;

  t = max4_d(base_e_p1.y, base_e_p2.y, query_e_p1.y, query_e_p2.y);
  if (y > t)
    y = t;

  xsect.x = x;
  xsect.y = y;

  gXsects[idx] = xsect;
}
