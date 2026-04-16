struct Intersection {
  double x;
  double y;
  uint eid0;
  uint eid1;
  int mid_point_polygon_id;
  int pad;
};

struct LaunchParams {
  int query_map_id;
  uint xsect_count;
  uint bit_shift;
  uint num_blocks;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Intersection> gXsectsIn;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> gBlockHist;   // [radix][block]

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

groupshared uint sHist[16];

uint getQueryEid(Intersection x, int query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 gid : SV_GroupID,
          uint3 gtid : SV_GroupThreadID) {
  const uint local_tid = gtid.x;
  const uint block_id = gid.x;

  if (local_tid < 16u) {
    sHist[local_tid] = 0u;
  }
  GroupMemoryBarrierWithGroupSync();

  const uint block_base = block_id * 1024u;

  [unroll]
  for (uint t = 0u; t < 4u; ++t) {
    const uint idx = block_base + local_tid + t * 256u;
    if (idx < gParams.xsect_count) {
      const Intersection x = gXsectsIn[idx];
      const uint key = getQueryEid(x, gParams.query_map_id);
      const uint digit = (key >> gParams.bit_shift) & 0xFu;
      InterlockedAdd(sHist[digit], 1u);
    }
  }

  GroupMemoryBarrierWithGroupSync();

  if (local_tid < 16u) {
    gBlockHist[local_tid * gParams.num_blocks + block_id] = sHist[local_tid];
  }
}