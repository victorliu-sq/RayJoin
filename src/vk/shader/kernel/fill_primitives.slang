import scaling;
import config;

struct Vec2i64 {
  int64_t x;
  int64_t y;
};

struct Int128 {
  uint64_t lo;
  int64_t hi;
};

struct Edge {
  Int128 a;
  Int128 b;
  Int128 c;

  uint eid;
  uint p1_idx;
  uint p2_idx;
  uint left_polygon_id;
  uint right_polygon_id;

  uint pad0;
  uint pad1;
  uint pad2;
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
  uint eid1;
  uint eid2;
};

struct PushConstants {
  uint numEdges;
  uint maxIter;
  float unused;
  uint pad;
};

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

[[vk::push_constant]]
PushConstants pc;

float next_float_from_double(double v, int dir, int iter) {
  float fv = (float)v;

  if (fv == 0.0f)
    return 0.0f;

  float inf = 1.0f / 0.0f;
  float to;

  if (v * (double)dir < 0.0) {
    to = 0.0f;
  } else {
    to = (dir > 0) ? inf : -inf;
  }

  for (int i = 0; i < iter; ++i) {
    fv = nextafter(fv, to);
  }

  return fv;
}

[numthreads(64, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint eid = tid.x;
  if (eid >= pc.numEdges)
    return;

  Scaling sc = gScaling[0];
  Edge e = edges[eid];

  Vec2i64 p1 = points[e.p1_idx];
  Vec2i64 p2 = points[e.p2_idx];

  double x1 = sc.UnscaleX(p1.x);
  double y1 = sc.UnscaleY(p1.y);
  double x2 = sc.UnscaleX(p2.x);
  double y2 = sc.UnscaleY(p2.y);

  AABB a;
  a.minX = next_float_from_double(min(x1, x2), -1, int(pc.maxIter));
  a.maxX = next_float_from_double(max(x1, x2), 1, int(pc.maxIter));
  a.minY = next_float_from_double(min(y1, y2), -1, int(pc.maxIter));
  a.maxY = next_float_from_double(max(y1, y2), 1, int(pc.maxIter));
  a.minZ = -PRIMITIVE_HEIGHT / 2.0f;
  a.maxZ = PRIMITIVE_HEIGHT / 2.0f;

  outAABB[eid] = a;

  Range r;
  r.eid1 = eid;
  r.eid2 = eid + 1;
  outRange[eid] = r;
}
