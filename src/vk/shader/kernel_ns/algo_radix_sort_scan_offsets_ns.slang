struct LaunchParams {
  uint num_blocks;
  uint _pad0;
  uint _pad1;
  uint _pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<uint> gBlockHist;      // [radix][block]

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> gBlockOffsets; // [radix][block]

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> gBucketBases;  // [radix]

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

groupshared uint sTotals[16];

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID) {
  const uint tid = gtid.x;

  if (tid < 16u) {
    uint running = 0u;
    const uint base = tid * gParams.num_blocks;

    for (uint b = 0u; b < gParams.num_blocks; ++b) {
      const uint c = gBlockHist[base + b];
      gBlockOffsets[base + b] = running;
      running += c;
    }

    sTotals[tid] = running;
  }

  GroupMemoryBarrierWithGroupSync();

  if (tid == 0u) {
    uint running = 0u;
    [unroll]
    for (uint r = 0u; r < 16u; ++r) {
      gBucketBases[r] = running;
      running += sTotals[r];
    }
  }
}