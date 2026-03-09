import edge;
import scaling;
import config;

struct Vec2i64 {
  int64_t x;
  int64_t y;
};

struct AABB {
  float minX;
  float minY;
  float minZ;
  float maxX;
  float maxY;
  float maxZ;
};

struct Range {
  uint eid1;  // inclusive
  uint eid2;  // exclusive
};

float next_float_from_double(double v, int dir, int iter) {
  float fv = (float)v;

  if (fv == 0.0f)
    return 0.0f;

  float inf = 1.0 / 0.0;

  float to = (v * dir < 0.0) ? 0.0 : (dir > 0 ? inf : -inf);

  for (int i = 0; i < iter; i++) {
    fv = nextafter(fv, to);
  }

  return fv;
}

[[vk::binding(0, 0)]]
StructuredBuffer<Vec2i64> points;

[[vk::binding(1, 0)]]
StructuredBuffer<Edge> edges;

[[vk::binding(2, 0)]]
RWStructuredBuffer<AABB> outAABB;

[[vk::binding(3, 0)]]
RWStructuredBuffer<Range> outRange;

[[vk::binding(4, 0)]]
StructuredBuffer<Scaling> gScaling;

struct PushConstants {
  uint numEdges;
  uint maxIter;
  float areaEnlarge;
};

[[vk::push_constant]]
PushConstants pc;

[numthreads(64, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID, uint3 ltid: SV_GroupThreadID, uint3 gid: SV_GroupID) {
  uint eid = gid.x * 64 + ltid.x;

  if (eid >= pc.numEdges) {
    return;
  }

  Scaling sc = gScaling[0];

  Edge e = edges[eid];

  Vec2i64 p1 = points[e.p1_idx];
  Vec2i64 p2 = points[e.p2_idx];

  // Unscale coordinates
  double x1 = sc.UnscaleX(p1.x);
  double y1 = sc.UnscaleY(p1.y);
  double x2 = sc.UnscaleX(p2.x);
  double y2 = sc.UnscaleY(p2.y);

  double minx = min(x1, x2);
  double maxx = max(x1, x2);
  double miny = min(y1, y2);
  double maxy = max(y1, y2);

  AABB a;

  a.minX = next_float_from_double(minx, -1, ROUNDING_ITER);
  a.maxX = next_float_from_double(maxx, 1, ROUNDING_ITER);

  a.minY = next_float_from_double(miny, -1, ROUNDING_ITER);
  a.maxY = next_float_from_double(maxy, 1, ROUNDING_ITER);

  a.minZ = -PRIMITIVE_HEIGHT * 0.5;
  a.maxZ = PRIMITIVE_HEIGHT * 0.5;

  outAABB[eid] = a;

  Range r;
  r.eid1 = eid;
  r.eid2 = eid + 1;

  outRange[eid] = r;
}
