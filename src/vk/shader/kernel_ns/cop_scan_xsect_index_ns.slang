// struct LaunchParams {
//   uint32_t unique_count;
//   uint32_t _pad0;
//   uint32_t _pad1;
//   uint32_t _pad2;
// };

// [[vk::binding(0, 0)]]
// StructuredBuffer<uint32_t> gNXsectsPerEdge;

// [[vk::binding(1, 0)]]
// RWStructuredBuffer<uint32_t> gXsectIndex;

// [[vk::push_constant]]
// ConstantBuffer<LaunchParams> gParams;

// [shader("compute")]
// [numthreads(1, 1, 1)]
// void main(uint3 tid : SV_DispatchThreadID) {
//   if (tid.x != 0) {
//     return;
//   }

//   gXsectIndex[0] = 0u;

//   uint32_t sum = 0u;
//   for (uint32_t i = 0u; i < gParams.unique_count; ++i) {
//     sum += gNXsectsPerEdge[i];
//     gXsectIndex[i + 1u] = sum;
//   }
// }

struct LaunchParams {
  uint32_t unique_count;
  uint32_t _pad0;
  uint32_t _pad1;
  uint32_t _pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<uint32_t> gNXsectsPerEdge;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint32_t> gXsectIndex;

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  if (tid.x != 0) {
    return;
  }

  gXsectIndex[0] = 0u;

  uint32_t sum = 0u;
  for (uint32_t i = 0u; i < gParams.unique_count; ++i) {
    sum += gNXsectsPerEdge[i];
    gXsectIndex[i + 1u] = sum;
  }
}
