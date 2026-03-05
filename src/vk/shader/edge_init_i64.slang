struct PointI64 {
  int64_t x;
  int64_t y;
};

struct Chain {
  int left_polygon_id;
  int right_polygon_id;
};

// uint => uint32 => 4 bytes
struct Edge {
  int64_t a;
  int64_t b;
  int64_t c;

  uint eid;
  uint p1_idx;
  uint p2_idx;
  uint left_polygon_id;
  uint right_polygon_id;
};

// [[vk::binding(# of binding, # of set)]]
[[vk::binding(0, 0)]]
StructuredBuffer<PointI64> gPoints;

[[vk::binding(1, 0)]]
StructuredBuffer<Chain> gChains;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> gRowIdx;

[[vk::binding(3, 0)]]
RWStructuredBuffer<Edge> gEdges;

struct Push {
  uint numPoints;
  uint numChains;
  uint numEdges;
  uint _pad;
};

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

[numthreads(256, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint pIdx = tid.x;
  if (pIdx >= pc.numPoints) {
    return;
  }

  uint chainId = findChain(pIdx);

  uint start = gRowIdx[chainId];
  uint end = gRowIdx[chainId + 1];

  if (pIdx >= end - 1) {
    return;
  }

  uint eid = pIdx - chainId;
  if (eid >= pc.numEdges) {
    return;
  }

  Chain ch = gChains[chainId];

  PointI64 p1 = gPoints[pIdx];
  PointI64 p2 = gPoints[pIdx + 1];

  int64_t a = p1.y - p2.y;
  int64_t b = p2.x - p1.x;
  int64_t c = -(p1.x * a) - (p1.y * b);

  if (b < 0) {
    a = -a;
    b = -b;
    c = -c;
  }

  Edge e;
  e.a = a;
  e.b = b;
  e.c = c;
  e.eid = eid;
  e.p1_idx = pIdx;
  e.p2_idx = pIdx + 1;
  e.left_polygon_id = ch.left_polygon_id;
  e.right_polygon_id = ch.right_polygon_id;

  gEdges[eid] = e;
}
