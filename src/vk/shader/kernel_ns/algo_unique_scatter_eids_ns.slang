typedef uint32_t index_t;
typedef int32_t  polygon_id_t;

struct Intersection {
  double x;
  double y;
  index_t eid0;
  index_t eid1;
  polygon_id_t mid_point_polygon_id;
};

struct LaunchParams {
  int32_t query_map_id;
  uint32_t xsect_count;
  uint32_t _pad0;
  uint32_t _pad1;
};

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[[vk::binding(0, 0)]]
StructuredBuffer<Intersection> gXsectsSorted;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> gIsUnique;

[[vk::binding(2, 0)]]
StructuredBuffer<uint32_t> gUniquePositions;

[[vk::binding(3, 0)]]
RWStructuredBuffer<index_t> gUniqueEids;

index_t getQueryEid(Intersection x, int32_t query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint32_t i = tid.x;
  if (i >= gParams.xsect_count) {
    return;
  }

  if (gIsUnique[i] == 0u) {
    return;
  }

  uint32_t out_idx = gUniquePositions[i];
  gUniqueEids[out_idx] = getQueryEid(gXsectsSorted[i], gParams.query_map_id);
}