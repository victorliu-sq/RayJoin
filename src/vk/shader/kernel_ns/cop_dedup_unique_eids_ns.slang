// struct Intersection {
//   double x;
//   double y;
//   uint64_t eid0;
//   uint64_t eid1;
//   uint32_t mid_point_polygon_id;
//   uint32_t pad;
// };

// struct LaunchParams {
//   int32_t query_map_id;
//   uint32_t xsect_count;
//   uint32_t _pad0;
//   uint32_t _pad1;
// };

// [[vk::push_constant]]
// ConstantBuffer<LaunchParams> gParams;

// [[vk::binding(0, 0)]]
// StructuredBuffer<Intersection> gXsectsSorted;

// [[vk::binding(1, 0)]]
// RWStructuredBuffer<uint64_t> gUniqueEids;

// [[vk::binding(2, 0)]]
// RWStructuredBuffer<uint32_t> gUniqueCount;

// uint64_t getQueryEid(Intersection x, int32_t query_map_id) {
//   return (query_map_id == 0) ? x.eid0 : x.eid1;
// }

// [shader("compute")]
// [numthreads(256, 1, 1)]
// void main(uint3 tid: SV_DispatchThreadID) {
//   uint32_t i = tid.x;
//   if (i >= gParams.xsect_count) {
//     return;
//   }

//   Intersection curr = gXsectsSorted[i];
//   uint64_t curr_eid = getQueryEid(curr, gParams.query_map_id);

//   bool is_unique = false;
//   if (i == 0u) {
//     is_unique = true;
//   } else {
//     Intersection prev = gXsectsSorted[i - 1u];
//     uint64_t prev_eid = getQueryEid(prev, gParams.query_map_id);
//     is_unique = (curr_eid != prev_eid);
//   }

//   if (is_unique) {
//     uint32_t out_idx = 0u;
//     InterlockedAdd(gUniqueCount[0], 1u, out_idx);
//     gUniqueEids[out_idx] = curr_eid;
//   }
// }

struct Intersection {
  double x;
  double y;
  uint eid0;
  uint eid1;
  int mid_point_polygon_id;
  int pad;
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
RWStructuredBuffer<uint> gUniqueEids;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint32_t> gUniqueCount;

uint getQueryEid(Intersection x, int32_t query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint32_t i = tid.x;
  if (i >= gParams.xsect_count) {
    return;
  }

  Intersection curr = gXsectsSorted[i];
  uint curr_eid = getQueryEid(curr, gParams.query_map_id);

  bool is_unique = false;
  if (i == 0u) {
    is_unique = true;
  } else {
    Intersection prev = gXsectsSorted[i - 1u];
    uint prev_eid = getQueryEid(prev, gParams.query_map_id);
    is_unique = (curr_eid != prev_eid);
  }

  if (is_unique) {
    uint32_t out_idx = 0u;
    InterlockedAdd(gUniqueCount[0], 1u, out_idx);
    gUniqueEids[out_idx] = curr_eid;
  }
}
