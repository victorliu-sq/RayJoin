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
  uint first;
  uint second;
};

struct PushConstants {
  uint numEdges;
  uint winSize;
  float areaEnlarge;
  uint pad;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Point> points;

[[vk::binding(1, 0)]]
StructuredBuffer<Edge> edges;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> inPrefix;

[[vk::binding(3, 0)]]
RWStructuredBuffer<AABB> outAABB;

[[vk::binding(4, 0)]]
RWStructuredBuffer<Range> outRange;

[[vk::push_constant]]
PushConstants pc;

AABB makeEdgeAABB(uint eid) {
  Edge e = edges[eid];
  Point p1 = points[e.p1_idx];
  Point p2 = points[e.p2_idx];

  double x1 = p1.x;
  double y1 = p1.y;
  double x2 = p2.x;
  double y2 = p2.y;

  AABB a;
  a.minX = float(min(x1, x2));
  a.maxX = float(max(x1, x2));
  a.minY = float(min(y1, y2));
  a.maxY = float(max(y1, y2));
  a.minZ = -PRIMITIVE_HEIGHT * 0.5f;
  a.maxZ =  PRIMITIVE_HEIGHT * 0.5f;
  return a;
}

AABB mergeAABB(AABB a, AABB b) {
  AABB r;
  r.minX = min(a.minX, b.minX);
  r.minY = min(a.minY, b.minY);
  r.minZ = min(a.minZ, b.minZ);
  r.maxX = max(a.maxX, b.maxX);
  r.maxY = max(a.maxY, b.maxY);
  r.maxZ = max(a.maxZ, b.maxZ);
  return r;
}

float aabbArea(AABB a) {
  float dx = max(0.0f, a.maxX - a.minX);
  float dy = max(0.0f, a.maxY - a.minY);
  return dx * dy;
}

float roundDown1(float v) { return nextafter(v, -1.0f / 0.0f); }
float roundUp1(float v)   { return nextafter(v,  1.0f / 0.0f); }

AABB roundAABBUp(AABB a) {
  a.minX = roundDown1(a.minX);
  a.minY = roundDown1(a.minY);
  a.minZ = roundDown1(a.minZ);
  a.maxX = roundUp1(a.maxX);
  a.maxY = roundUp1(a.maxY);
  a.maxZ = roundUp1(a.maxZ);
  return a;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint winId = tid.x;
  uint nWins = (pc.numEdges + pc.winSize - 1u) / pc.winSize;

  if (winId >= nWins)
    return;

  uint eidBegin = min(winId * pc.winSize, pc.numEdges);
  uint eidEnd   = min((winId + 1u) * pc.winSize, pc.numEdges);
  uint gidBegin = inPrefix[winId];

  if (eidBegin >= eidEnd)
    return;

  AABB lastAABB;
  uint lastEid = eidBegin;
  uint localGroupCount = 0u;

  for (uint eid = eidBegin; eid < eidEnd; ++eid) {
    AABB curr = makeEdgeAABB(eid);

    if (eid == eidBegin) {
      lastAABB = curr;
      lastEid = eid;
    } else {
      AABB merged = mergeAABB(lastAABB, curr);
      float area = aabbArea(lastAABB);
      float mergedArea = aabbArea(merged);

      bool canMerge = false;
      if (area <= 0.0f) {
        canMerge = (mergedArea <= 0.0f);
      } else {
        canMerge = ((mergedArea / area) < pc.areaEnlarge);
      }

      if (canMerge) {
        lastAABB = merged;
      } else {
        outAABB[gidBegin + localGroupCount] = roundAABBUp(lastAABB);

        Range r;
        r.first = lastEid;
        r.second = eid;
        outRange[gidBegin + localGroupCount] = r;

        lastAABB = curr;
        lastEid = eid;
        localGroupCount += 1u;
      }
    }
  }

  outAABB[gidBegin + localGroupCount] = roundAABBUp(lastAABB);

  Range r;
  r.first = lastEid;
  r.second = eidEnd;
  outRange[gidBegin + localGroupCount] = r;
}