struct LaunchParams {
  uint32_t unique_count;
  uint32_t _pad0;
  uint32_t _pad1;
  uint32_t _pad2;
};

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[[vk::binding(0, 0)]]
StructuredBuffer<uint32_t> gNXsectsPerEdge;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> gUniquePositions;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint32_t> gXsectIndex;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint32_t i = tid.x;

  if (i == 0u) {
    gXsectIndex[0] = 0u;
  }

  if (i >= gParams.unique_count) {
    return;
  }

  gXsectIndex[i + 1u] = gUniquePositions[i] + gNXsectsPerEdge[i];
}