module edge_init_ns_d64;

struct PointD {
  double x;
  double y;
};

struct Chain {
  int64_t id;
  int64_t first_point_idx;
  int64_t last_point_idx;
  int64_t left_polygon_id;
  int64_t right_polygon_id;
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

struct Push {
  uint numPoints;
  uint numChains;
  uint numEdges;
  uint _pad;
};

[[vk::binding(0, 0)]]
StructuredBuffer<PointD> gPoints;

[[vk::binding(1, 0)]]
StructuredBuffer<Chain> gChains;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> gRowIdx;

[[vk::binding(3, 0)]]
RWStructuredBuffer<EdgeD> gEdges;

[[vk::push_constant]]
Push pc;

uint findChain(uint pIdx) {
  uint lo = 0;
  uint hi = pc.numChains;

  while (lo + 1 < hi) {
    uint mid = (lo + hi) >> 1;
    if (gRowIdx[mid] <= pIdx)
      lo = mid;
    else
      hi = mid;
  }

  return lo;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint pIdx = tid.x;

  if (pIdx >= pc.numPoints)
    return;

  uint chainId = findChain(pIdx);
  uint end = gRowIdx[chainId + 1];

  if (pIdx >= end - 1)
    return;

  uint eid = pIdx - chainId;
  if (eid >= pc.numEdges)
    return;

  Chain ch = gChains[chainId];
  PointD p1 = gPoints[pIdx];
  PointD p2 = gPoints[pIdx + 1];

  double a = p1.y - p2.y;
  double b = p2.x - p1.x;
  double c = -(p1.x * a) - (p1.y * b);

  if (b < 0.0 || (b == 0.0 && a < 0.0)) {
    a = -a;
    b = -b;
    c = -c;
  }

  EdgeD e;
  e.a = a;
  e.b = b;
  e.c = c;
  e.eid = eid;
  e.p1_idx = pIdx;
  e.p2_idx = pIdx + 1;
  e.left_polygon_id = (uint)ch.left_polygon_id;
  e.right_polygon_id = (uint)ch.right_polygon_id;

  gEdges[eid] = e;
}
