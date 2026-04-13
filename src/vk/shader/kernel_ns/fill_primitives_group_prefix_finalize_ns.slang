struct PushConstants {
  uint winCount;
  uint _pad0;
  uint _pad1;
  uint _pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<uint> inNumGroups;

[[vk::binding(1, 0)]]
StructuredBuffer<uint> inExclusivePos;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> outPrefix;

[[vk::push_constant]]
PushConstants pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint i = tid.x;

  if (i >= pc.winCount)
    return;

  if (i == 0u) {
    outPrefix[0] = 0u;
  }

  outPrefix[i + 1u] = inExclusivePos[i] + inNumGroups[i];
}