struct LaunchParams {
  uint32_t xsect_count;
  uint32_t _pad0;
  uint32_t _pad1;
  uint32_t _pad2;
};

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[[vk::binding(0, 0)]]
StructuredBuffer<uint32_t> gIsUnique;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> gUniquePositions;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint32_t> gUniqueCount;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  if (tid.x != 0u) {
    return;
  }

  if (gParams.xsect_count == 0u) {
    gUniqueCount[0] = 0u;
    return;
  }

  uint32_t last = gParams.xsect_count - 1u;
  gUniqueCount[0] = gUniquePositions[last] + gIsUnique[last];
}