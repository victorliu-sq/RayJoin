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
RWStructuredBuffer<uint32_t> gData;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> gBlockOffsets;

[shader("compute")]
[numthreads(BLOCK_SIZE, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID,
          uint3 gid : SV_GroupID)
{
  uint global_idx = tid.x;
  if (global_idx >= gParams.count) {
    return;
  }

  uint block_idx = gid.x;
  gData[global_idx] += gBlockOffsets[block_idx];
}