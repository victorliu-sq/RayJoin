import constants;

StructuredBuffer<uint> elementCounts : register(t0, space0);
RWStructuredBuffer<uint> globalHistogram : register(u1, space0);
RWStructuredBuffer<uint> partitionHistogram : register(u2, space0);
StructuredBuffer<uint> keys : register(t3, space0);

groupshared uint localHistogram[RADIX];

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID,
          uint3 groupId : SV_GroupID,
          uniform int pass) {
  uint elementCount = elementCounts[0];

  uint index = groupThreadID.x;
  uint partitionIndex = groupId.x;
  uint partitionStart = partitionIndex * PARTITION_SIZE;

  if (partitionStart >= elementCount) {
    return;
  }

  if (index < RADIX) {
    localHistogram[index] = 0;
  }
  GroupMemoryBarrierWithGroupSync();

  for (int i = 0; i < PARTITION_DIVISION; ++i) {
    uint keyIndex = partitionStart + WORKGROUP_SIZE * i + index;
    uint key = keyIndex < elementCount ? keys[keyIndex] : 0xffffffff;
    uint radix = bitfieldExtract(key, 8 * pass, 8);
    __atomic_add(localHistogram[radix], 1, MemoryOrder.Relaxed);
  }
  GroupMemoryBarrierWithGroupSync();

  if (index < RADIX) {
    partitionHistogram[RADIX * partitionIndex + index] = localHistogram[index];
    __atomic_add(globalHistogram[RADIX * pass + index], localHistogram[index], MemoryOrder.Relaxed);
  }
}