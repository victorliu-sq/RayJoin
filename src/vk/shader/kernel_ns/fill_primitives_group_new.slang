struct Edge {
  int64_t a;
  int64_t b;
  int64_t c;
  uint64_t eid;
  uint64_t p1_idx;
  uint64_t p2_idx;
  uint64_t left_polygon_id;
  uint64_t right_polygon_id;
};

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
  uint x;
  uint y;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Vec2i64> points;

[[vk::binding(1, 0)]]
StructuredBuffer<Edge> edges;

[[vk::binding(2, 0)]]
RWStructuredBuffer<AABB> outAABB;

[[vk::binding(3, 0)]]
RWStructuredBuffer<Range> outRange;

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
  if (eid >= pc.numEdges)
    return;

  Edge e = edges[eid];

  Vec2i64 p1 = points[e.p1_idx];
  Vec2i64 p2 = points[e.p2_idx];

  float minx = float(min(p1.x, p2.x));
  float maxx = float(max(p1.x, p2.x));
  float miny = float(min(p1.y, p2.y));
  float maxy = float(max(p1.y, p2.y));

  AABB a;
  a.minX = minx;
  a.minY = miny;
  a.minZ = 0;
  a.maxX = maxx;
  a.maxY = maxy;
  a.maxZ = 0;

  outAABB[eid] = a;

  Range r;
  r.x = eid;
  r.y = eid + 1;

  outRange[eid] = r;
}
