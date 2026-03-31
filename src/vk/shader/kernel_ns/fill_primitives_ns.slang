// // Integer constants
// public static const int DONTKNOW = -1;
// public static const int SCALING_BOUNDING_BOX_MARGIN = 1;
// public static const int ROUNDING_ITER = 2;
// public static const int EXTERIOR_FACE_ID = 0;

// // Floating point constants
// public static const float RAY_TMAX = 1e10;
// public static const float PRIMITIVE_HEIGHT = 0.01;

// struct Edge {
//   double a;
//   double b;
//   double c;

//   uint64_t eid;
//   uint64_t p1_idx;
//   uint64_t p2_idx;
//   uint64_t left_polygon_id;
//   uint64_t right_polygon_id;
// };

// struct Vec2d {
//   double x;
//   double y;
// };

// struct AABB {
//   float minX;
//   float minY;
//   float minZ;
//   float maxX;
//   float maxY;
//   float maxZ;
// };

// struct Range {
//   uint eid1;  // inclusive
//   uint eid2;  // exclusive
// };

// float nextFloatFromDouble(double v, int dir, uint iter) {
//   float fv = (float)v;

//   if (fv == 0.0f)
//     return 0.0f;

//   float inf = 1.0f / 0.0f;
//   float to = (v * (double)dir < 0.0) ? 0.0f : ((dir > 0) ? inf : -inf);

//   for (uint i = 0; i < iter; ++i) {
//     fv = nextafter(fv, to);
//   }

//   return fv;
// }

// [[vk::binding(0, 0)]]
// StructuredBuffer<Vec2d> points;

// [[vk::binding(1, 0)]]
// StructuredBuffer<Edge> edges;

// [[vk::binding(2, 0)]]
// RWStructuredBuffer<AABB> outAABB;

// [[vk::binding(3, 0)]]
// RWStructuredBuffer<Range> outRange;

// struct PushConstants {
//   uint numEdges;
//   uint maxIter;
//   float areaEnlarge;
// };

// [[vk::push_constant]]
// PushConstants pc;

// [shader("compute")]
// [numthreads(64, 1, 1)]
// void main(uint3 tid: SV_DispatchThreadID, uint3 ltid: SV_GroupThreadID, uint3 gid: SV_GroupID) {
//   uint eid = gid.x * 64 + ltid.x;

//   if (eid >= pc.numEdges)
//     return;

//   Edge e = edges[eid];

//   Vec2d p1 = points[e.p1_idx];
//   Vec2d p2 = points[e.p2_idx];

//   double minx = min(p1.x, p2.x);
//   double maxx = max(p1.x, p2.x);
//   double miny = min(p1.y, p2.y);
//   double maxy = max(p1.y, p2.y);

//   double dx = maxx - minx;
//   double dy = maxy - miny;

//   double padX = dx * (double)pc.areaEnlarge;
//   double padY = dy * (double)pc.areaEnlarge;

//   minx -= padX;
//   maxx += padX;
//   miny -= padY;
//   maxy += padY;

//   AABB a;
//   a.minX = nextFloatFromDouble(minx, -1, pc.maxIter);
//   a.maxX = nextFloatFromDouble(maxx, 1, pc.maxIter);
//   a.minY = nextFloatFromDouble(miny, -1, pc.maxIter);
//   a.maxY = nextFloatFromDouble(maxy, 1, pc.maxIter);

//   a.minZ = -PRIMITIVE_HEIGHT * 0.5f;
//   a.maxZ = PRIMITIVE_HEIGHT * 0.5f;

//   outAABB[eid] = a;

//   Range r;
//   r.eid1 = eid;
//   r.eid2 = eid + 1;
//   outRange[eid] = r;
// }

// Integer constants
public static const int DONTKNOW = -1;
public static const int SCALING_BOUNDING_BOX_MARGIN = 1;
public static const int ROUNDING_ITER = 2;
public static const int EXTERIOR_FACE_ID = 0;

// Floating point constants
public static const float RAY_TMAX = 1e10;
public static const float PRIMITIVE_HEIGHT = 0.01;

struct Edge {
  double a;
  double b;
  double c;

  uint64_t eid;
  uint64_t p1_idx;
  uint64_t p2_idx;
  uint64_t left_polygon_id;
  uint64_t right_polygon_id;
};

struct Vec2d {
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
StructuredBuffer<Vec2d> points;

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

  if (v * (double)dir < 0.0) {
    to = 0.0f;
  } else {
    to = (dir > 0) ? inf : -inf;
  }

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

  Vec2d p1 = points[e.p1_idx];
  Vec2d p2 = points[e.p2_idx];

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
