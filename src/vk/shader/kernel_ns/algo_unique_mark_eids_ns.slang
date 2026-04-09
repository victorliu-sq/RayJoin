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
RWStructuredBuffer<uint32_t> gIsUnique;

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

  if (i == 0u) {
    gIsUnique[i] = 1u;
    return;
  }

  index_t prev_eid = getQueryEid(gXsectsSorted[i - 1u], gParams.query_map_id);
  index_t curr_eid = getQueryEid(gXsectsSorted[i], gParams.query_map_id);
  gIsUnique[i] = (curr_eid != prev_eid) ? 1u : 0u;
}