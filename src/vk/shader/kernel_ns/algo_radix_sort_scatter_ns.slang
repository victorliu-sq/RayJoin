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
RWStructuredBuffer<Intersection> gXsectsOut;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> gBlockOffsets;   // [radix][block]

[[vk::binding(3, 0)]]
StructuredBuffer<uint> gBucketBases;    // [radix]

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

groupshared uint sFlags[1024];
groupshared uint sScanA[1024];
groupshared uint sScanB[1024];

uint getQueryEid(Intersection x, int query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

uint exclusiveScan1024(uint local_pos, uint valid_count) {
  sScanA[local_pos] = (local_pos < valid_count) ? sFlags[local_pos] : 0u;
  GroupMemoryBarrierWithGroupSync();

  for (uint offset = 1u; offset < 1024u; offset <<= 1u) {
    uint v = sScanA[local_pos];
    if (local_pos >= offset) {
      v += sScanA[local_pos - offset];
    }
    sScanB[local_pos] = v;
    GroupMemoryBarrierWithGroupSync();

    sScanA[local_pos] = sScanB[local_pos];
    GroupMemoryBarrierWithGroupSync();
  }

  const uint inclusive = (local_pos < valid_count) ? sScanA[local_pos] : 0u;
  return (local_pos < valid_count) ? (inclusive - sFlags[local_pos]) : 0u;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 gid : SV_GroupID,
          uint3 gtid : SV_GroupThreadID) {
  const uint local_tid = gtid.x;
  const uint block_id = gid.x;
  const uint block_base = block_id * 1024u;

  const uint idx0 = block_base + local_tid + 0u * 256u;
  const uint idx1 = block_base + local_tid + 1u * 256u;
  const uint idx2 = block_base + local_tid + 2u * 256u;
  const uint idx3 = block_base + local_tid + 3u * 256u;

  Intersection x0, x1, x2, x3;
  bool v0 = idx0 < gParams.xsect_count;
  bool v1 = idx1 < gParams.xsect_count;
  bool v2 = idx2 < gParams.xsect_count;
  bool v3 = idx3 < gParams.xsect_count;

  if (v0) x0 = gXsectsIn[idx0];
  if (v1) x1 = gXsectsIn[idx1];
  if (v2) x2 = gXsectsIn[idx2];
  if (v3) x3 = gXsectsIn[idx3];

  uint d0 = v0 ? ((getQueryEid(x0, gParams.query_map_id) >> gParams.bit_shift) & 0xFu) : 0u;
  uint d1 = v1 ? ((getQueryEid(x1, gParams.query_map_id) >> gParams.bit_shift) & 0xFu) : 0u;
  uint d2 = v2 ? ((getQueryEid(x2, gParams.query_map_id) >> gParams.bit_shift) & 0xFu) : 0u;
  uint d3 = v3 ? ((getQueryEid(x3, gParams.query_map_id) >> gParams.bit_shift) & 0xFu) : 0u;

  const uint valid_count =
      (gParams.xsect_count > block_base) ? min(1024u, gParams.xsect_count - block_base) : 0u;

  [unroll]
  for (uint radix = 0u; radix < 16u; ++radix) {
    const uint p0 = local_tid + 0u * 256u;
    const uint p1 = local_tid + 1u * 256u;
    const uint p2 = local_tid + 2u * 256u;
    const uint p3 = local_tid + 3u * 256u;

    sFlags[p0] = (v0 && d0 == radix) ? 1u : 0u;
    sFlags[p1] = (v1 && d1 == radix) ? 1u : 0u;
    sFlags[p2] = (v2 && d2 == radix) ? 1u : 0u;
    sFlags[p3] = (v3 && d3 == radix) ? 1u : 0u;

    GroupMemoryBarrierWithGroupSync();

    const uint r0 = exclusiveScan1024(p0, valid_count);
    const uint r1 = exclusiveScan1024(p1, valid_count);
    const uint r2 = exclusiveScan1024(p2, valid_count);
    const uint r3 = exclusiveScan1024(p3, valid_count);

    const uint block_offset = gBlockOffsets[radix * gParams.num_blocks + block_id];
    const uint bucket_base = gBucketBases[radix];

    if (v0 && d0 == radix) {
      gXsectsOut[bucket_base + block_offset + r0] = x0;
    }
    if (v1 && d1 == radix) {
      gXsectsOut[bucket_base + block_offset + r1] = x1;
    }
    if (v2 && d2 == radix) {
      gXsectsOut[bucket_base + block_offset + r2] = x2;
    }
    if (v3 && d3 == radix) {
      gXsectsOut[bucket_base + block_offset + r3] = x3;
    }

    GroupMemoryBarrierWithGroupSync();
  }
}