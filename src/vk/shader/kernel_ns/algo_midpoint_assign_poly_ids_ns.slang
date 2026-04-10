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
  uint32_t unique_count;
  uint32_t _pad0;
  uint32_t _pad1;
  uint32_t _pad2;
};

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[[vk::binding(0, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> gXsectIndex;

[[vk::binding(2, 0)]]
StructuredBuffer<polygon_id_t> gMidPolyIds;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint32_t group_idx = tid.x;
  if (group_idx >= gParams.unique_count) {
    return;
  }

  uint32_t begin = gXsectIndex[group_idx];
  uint32_t end = gXsectIndex[group_idx + 1u];
  uint32_t n_xsect = end - begin;

  if (n_xsect <= 1u) {
    return;
  }

  for (uint32_t local_idx = 0u; local_idx + 1u < n_xsect; ++local_idx) {
    uint32_t midpoint_idx = begin - group_idx + local_idx;
    gXsects[begin + local_idx].mid_point_polygon_id = gMidPolyIds[midpoint_idx];
  }
}