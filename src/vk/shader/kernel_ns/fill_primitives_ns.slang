public static const int DONTKNOW = -1;
public static const int SCALING_BOUNDING_BOX_MARGIN = 1;
public static const int ROUNDING_ITER = 2;
public static const int EXTERIOR_FACE_ID = 0;

public static const float RAY_TMAX = 1e10;
public static const float PRIMITIVE_HEIGHT = 0.01;

struct Edge {
  double a;
  double b;
  double c;

  uint eid;
  uint p1_idx;
  uint p2_idx;
  uint left_polygon_id;
  uint right_polygon_id;
};

struct Point {
  double x;
  double y;
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
StructuredBuffer<Point> points;

[[vk::binding(1, 0)]]
StructuredBuffer<Edge> edges;

[[vk::binding(2, 0)]]
RWStructuredBuffer<AABB> outAABB;

[[vk::binding(3, 0)]]
RWStructuredBuffer<Range> outRange;

[[vk::push_constant]]
PushConstants pc;

float nextFloatFromDouble(double v, int dir, uint iter) {
  float fv = (float)v;

  if (fv == 0.0f)
    return 0.0f;

  float inf = 1.0f / 0.0f;
  float to;

  if (v * (double)dir < 0.0)
    to = 0.0f;
  else
    to = (dir > 0) ? inf : -inf;

  for (uint i = 0; i < iter; ++i) {
    fv = nextafter(fv, to);
  }

  return fv;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint eid = tid.x;

  if (eid >= pc.numEdges)
    return;

  Edge e = edges[eid];

  Point p1 = points[e.p1_idx];
  Point p2 = points[e.p2_idx];

  double x1 = p1.x;
  double y1 = p1.y;
  double x2 = p2.x;
  double y2 = p2.y;

  AABB a;
  a.minX = nextFloatFromDouble(min(x1, x2), -1, pc.maxIter);
  a.maxX = nextFloatFromDouble(max(x1, x2), 1, pc.maxIter);
  a.minY = nextFloatFromDouble(min(y1, y2), -1, pc.maxIter);
  a.maxY = nextFloatFromDouble(max(y1, y2), 1, pc.maxIter);
  a.minZ = -PRIMITIVE_HEIGHT / 2.0f;
  a.maxZ = PRIMITIVE_HEIGHT / 2.0f;

  outAABB[eid] = a;

  Range r;
  r.eid1 = eid;
  r.eid2 = eid + 1;
  outRange[eid] = r;
}
