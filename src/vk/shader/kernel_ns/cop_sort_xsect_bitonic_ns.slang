struct Intersection {
  double x;
  double y;
  uint64_t eid0;
  uint64_t eid1;
  uint32_t mid_point_polygon_id;
  uint32_t pad;
};

struct XsectSortEntry {
  uint64_t query_eid;
  uint32_t src_idx;
  uint32_t _pad0;
};

struct LaunchParams {
  uint32_t j;
  uint32_t k;
  uint32_t count;
  uint32_t _pad0;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<XsectSortEntry> gSortEntries;

[[vk::binding(1, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

bool lessEntry(XsectSortEntry a, XsectSortEntry b) {
  if (a.query_eid < b.query_eid)
    return true;
  if (b.query_eid < a.query_eid)
    return false;
  return a.src_idx < b.src_idx;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint32_t i = tid.x;
  if (i >= gParams.count) {
    return;
  }

  uint32_t ixj = i ^ gParams.j;
  if (ixj <= i || ixj >= gParams.count) {
    return;
  }

  bool ascending = ((i & gParams.k) == 0);

  XsectSortEntry a = gSortEntries[i];
  XsectSortEntry b = gSortEntries[ixj];

  bool a_less_b = lessEntry(a, b);
  bool b_less_a = lessEntry(b, a);

  bool should_swap = ascending ? b_less_a : a_less_b;

  if (should_swap) {
    gSortEntries[i] = b;
    gSortEntries[ixj] = a;

    Intersection xa = gXsects[i];
    Intersection xb = gXsects[ixj];
    gXsects[i] = xb;
    gXsects[ixj] = xa;
  }
}
