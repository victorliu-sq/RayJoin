struct PointI64 {
  int64_t x;
  int64_t y;
};

struct Chain {
  int left_polygon_id;
  int right_polygon_id;
};

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

struct Push {
  uint64_t pointsAddr;
  uint64_t chainsAddr;
  uint64_t rowIdxAddr;
  uint64_t edgesAddr;

  uint numPoints;
  uint numChains;
  uint numEdges;
  uint pad;
};

[[vk::push_constant]]
Push pc;

PointI64* gPoints() {
  return (PointI64*) pc.pointsAddr;
}
Chain* gChains() {
  return (Chain*) pc.chainsAddr;
}
uint* gRowIdx() {
  return (uint*) pc.rowIdxAddr;
}
Edge* gEdges() {
  return (Edge*) pc.edgesAddr;
}

uint findChain(uint pIdx) {
  uint lo = 0;
  uint hi = pc.numChains;

  uint* row = gRowIdx();

  while (lo + 1 < hi) {
    uint mid = (lo + hi) >> 1;

    if (row[mid] <= pIdx)
      lo = mid;
    else
      hi = mid;
  }

  return lo;
}

[numthreads(256, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint pIdx = tid.x;

  if (pIdx >= pc.numPoints)
    return;

  PointI64* points = gPoints();
  Chain* chains = gChains();
  uint* row = gRowIdx();
  Edge* edges = gEdges();

  uint chainId = findChain(pIdx);

  uint start = row[chainId];
  uint end = row[chainId + 1];

  if (pIdx >= end - 1)
    return;

  uint eid = pIdx - chainId;

  if (eid >= pc.numEdges)
    return;

  Chain ch = chains[chainId];

  PointI64 p1 = points[pIdx];
  PointI64 p2 = points[pIdx + 1];

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

  edges[eid] = e;
}
