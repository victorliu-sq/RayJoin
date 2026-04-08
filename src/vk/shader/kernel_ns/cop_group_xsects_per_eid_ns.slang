struct Intersection {
  double x;
  double y;
  uint64_t eid0;
  uint64_t eid1;
  uint32_t mid_point_polygon_id;
  uint32_t pad;
};

struct LaunchParams {
  int32_t query_map_id;
  uint32_t xsect_count;
  uint32_t unique_count;
  uint32_t _pad0;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Intersection> gXsectsSorted;

[[vk::binding(1, 0)]]
StructuredBuffer<uint64_t> gUniqueEids;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint32_t> gNXsectsPerEdge;

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

uint64_t getQueryEid(Intersection x, int32_t query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

uint32_t lowerBound(uint64_t eid) {
  uint32_t lo = 0u;
  uint32_t hi = gParams.xsect_count;
  while (lo < hi) {
    uint32_t mid = lo + ((hi - lo) >> 1u);
    uint64_t mid_eid = getQueryEid(gXsectsSorted[mid], gParams.query_map_id);
    if (mid_eid < eid) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }
  return lo;
}

uint32_t upperBound(uint64_t eid) {
  uint32_t lo = 0u;
  uint32_t hi = gParams.xsect_count;
  while (lo < hi) {
    uint32_t mid = lo + ((hi - lo) >> 1u);
    uint64_t mid_eid = getQueryEid(gXsectsSorted[mid], gParams.query_map_id);
    if (mid_eid <= eid) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }
  return lo;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint32_t i = tid.x;
  if (i >= gParams.unique_count) {
    return;
  }

  uint64_t eid = gUniqueEids[i];
  uint32_t first = lowerBound(eid);
  uint32_t last = upperBound(eid);
  gNXsectsPerEdge[i] = last - first;
}