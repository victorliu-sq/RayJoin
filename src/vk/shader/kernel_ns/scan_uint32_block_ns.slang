static const uint BLOCK_SIZE = 256u;

struct LaunchParams {
  uint32_t count;
  uint32_t _pad0;
  uint32_t _pad1;
  uint32_t _pad2;
};

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[[vk::binding(0, 0)]]
StructuredBuffer<uint32_t> gIn;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint32_t> gOut;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint32_t> gBlockSums;

groupshared uint sData[BLOCK_SIZE];

[shader("compute")]
[numthreads(BLOCK_SIZE, 1, 1)]
void main(uint3 tid  : SV_DispatchThreadID,
          uint3 ltid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID)
{
  uint local_idx = ltid.x;
  uint global_idx = tid.x;
  uint block_idx = gid.x;

  uint v = 0u;
  if (global_idx < gParams.count) {
    v = gIn[global_idx];
  }
  sData[local_idx] = v;
  GroupMemoryBarrierWithGroupSync();

  // Upsweep
  for (uint offset = 1u; offset < BLOCK_SIZE; offset <<= 1u) {
    uint idx = ((local_idx + 1u) * offset * 2u) - 1u;
    if (idx < BLOCK_SIZE) {
      sData[idx] += sData[idx - offset];
    }
    GroupMemoryBarrierWithGroupSync();
  }

  // Save total and convert to exclusive
  if (local_idx == 0u) {
    gBlockSums[block_idx] = sData[BLOCK_SIZE - 1u];
    sData[BLOCK_SIZE - 1u] = 0u;
  }
  GroupMemoryBarrierWithGroupSync();

  // Downsweep
  for (uint offset = BLOCK_SIZE >> 1u; offset > 0u; offset >>= 1u) {
    uint idx = ((local_idx + 1u) * offset * 2u) - 1u;
    if (idx < BLOCK_SIZE) {
      uint t = sData[idx - offset];
      sData[idx - offset] = sData[idx];
      sData[idx] += t;
    }
    GroupMemoryBarrierWithGroupSync();
  }

  if (global_idx < gParams.count) {
    gOut[global_idx] = sData[local_idx];
  }
}