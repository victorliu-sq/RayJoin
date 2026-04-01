struct PointI64 {
  int64_t x;
  int64_t y;
};

struct Chain {
  int64_t id;
  int64_t first_point_idx;
  int64_t last_point_idx;
  int64_t left_polygon_id;
  int64_t right_polygon_id;
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

struct Push {
  uint num_points;
  uint num_chains;
  uint num_edges;
  uint _pad;
};

[[vk::push_constant]]
Push pc;

[[vk::binding(0, 0)]]
StructuredBuffer<PointI64> gPoints;

[[vk::binding(1, 0)]]
StructuredBuffer<Chain> gChains;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> gRowIdx;

[[vk::binding(3, 0)]]
RWStructuredBuffer<Edge> gEdges;

uint findChain(uint pIdx) {
  uint lo = 0;
  uint hi = pc.num_chains;

  while (lo + 1 < hi) {
    uint mid = (lo + hi) >> 1;
    if (gRowIdx[mid] <= pIdx)
      lo = mid;
    else
      hi = mid;
  }

  return lo;
}

Int128 MakeInt128(int64_t hi, uint64_t lo) {
  Int128 v;
  v.lo = lo;
  v.hi = hi;
  return v;
}

Int128 FromI64(int64_t x) {
  return MakeInt128(x < 0 ? -1ll : 0ll, (uint64_t)x);
}

Int128 BitNot(Int128 x) {
  return MakeInt128(~x.hi, ~x.lo);
}

Int128 Add128(Int128 a, Int128 b) {
  Int128 r;
  r.lo = a.lo + b.lo;
  uint64_t carry = (r.lo < a.lo) ? 1ull : 0ull;
  r.hi = a.hi + b.hi + (int64_t)carry;
  return r;
}

Int128 Neg128(Int128 x) {
  return Add128(BitNot(x), MakeInt128(0, 1));
}

Int128 MulU64U64(uint64_t x, uint64_t y) {
  uint64_t x0 = x & 0xffffffffull;
  uint64_t x1 = x >> 32;
  uint64_t y0 = y & 0xffffffffull;
  uint64_t y1 = y >> 32;

  uint64_t p00 = x0 * y0;
  uint64_t p01 = x0 * y1;
  uint64_t p10 = x1 * y0;
  uint64_t p11 = x1 * y1;

  uint64_t middle = (p00 >> 32) + (p01 & 0xffffffffull) + (p10 & 0xffffffffull);
  uint64_t lo = (p00 & 0xffffffffull) | (middle << 32);
  uint64_t hi = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);

  return MakeInt128((int64_t)hi, lo);
}

Int128 MulI64I64(int64_t a, int64_t b) {
  bool neg = ((a < 0) != (b < 0));

  uint64_t ua = (a < 0) ? (uint64_t)(-a) : (uint64_t)a;
  uint64_t ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;

  Int128 r = MulU64U64(ua, ub);
  return neg ? Neg128(r) : r;
}

[numthreads(64, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint pIdx = tid.x;

  if (pIdx >= pc.num_points)
    return;

  uint chainId = findChain(pIdx);
  uint end = gRowIdx[chainId + 1];

  if (pIdx >= end - 1)
    return;

  uint eid = pIdx - chainId;
  if (eid >= pc.num_edges)
    return;

  Chain ch = gChains[chainId];
  PointI64 p1 = gPoints[pIdx];
  PointI64 p2 = gPoints[pIdx + 1];

  int64_t a64 = p1.y - p2.y;
  int64_t b64 = p2.x - p1.x;

  Int128 a = FromI64(a64);
  Int128 b = FromI64(b64);
  Int128 c = Neg128(Add128(MulI64I64(p1.x, a64), MulI64I64(p1.y, b64)));

  if (b64 < 0) {
    a = Neg128(a);
    b = Neg128(b);
    c = Neg128(c);
  }

  Edge e;
  e.a = a;
  e.b = b;
  e.c = c;
  e.eid = eid;
  e.p1_idx = pIdx;
  e.p2_idx = pIdx + 1;
  e.left_polygon_id = (uint)ch.left_polygon_id;
  e.right_polygon_id = (uint)ch.right_polygon_id;
  e.pad0 = 0;
  e.pad1 = 0;
  e.pad2 = 0;

  gEdges[eid] = e;
}
