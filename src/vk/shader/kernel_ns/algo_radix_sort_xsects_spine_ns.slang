import constants;

StructuredBuffer<uint> elementCounts : register(t0, space0);
RWStructuredBuffer<uint> globalHistogram : register(u1, space0);
RWStructuredBuffer<uint> partitionHistogram : register(u2, space0);

groupshared uint reduction;
groupshared uint intermediate[MAX_SUBGROUP_SIZE];

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID,
          uint3 groupId : SV_GroupID,
          uint groupIndex : SV_GroupIndex,
          uniform int pass) {
  uint elementCount = elementCounts[0];

  uint laneIndex = WaveGetLaneIndex();
  uint laneCount = WaveGetLaneCount();
  uint waveIndex = groupIndex / laneCount;
  uint waveCount = WORKGROUP_SIZE / laneCount;
  uint index = waveIndex * laneCount + laneIndex;

  uint radix = groupId.x;
  uint partitionCount = (elementCount + PARTITION_SIZE - 1) / PARTITION_SIZE;

  if (index == 0) {
    reduction = 0;
  }
  GroupMemoryBarrierWithGroupSync();

  for (uint i = 0; WORKGROUP_SIZE * i < partitionCount; ++i) {
    uint partitionIndex = WORKGROUP_SIZE * i + index;
    uint value =
        partitionIndex < partitionCount ? partitionHistogram[RADIX * partitionIndex + radix] : 0;
    uint excl = WavePrefixSum(value) + reduction;
    uint sum = WaveActiveSum(value);

    if (WaveIsFirstLane()) {
      intermediate[waveIndex] = sum;
    }
    GroupMemoryBarrierWithGroupSync();

    if (index < waveCount) {
      uint excl2 = WavePrefixSum(intermediate[index]);
      uint sum2 = WaveActiveSum(intermediate[index]);
      intermediate[index] = excl2;

      if (index == 0) {
        reduction += sum2;
      }
    }
    GroupMemoryBarrierWithGroupSync();

    if (partitionIndex < partitionCount) {
      excl += intermediate[waveIndex];
      partitionHistogram[RADIX * partitionIndex + radix] = excl;
    }
    GroupMemoryBarrierWithGroupSync();
  }

  if (radix == 0) {
    if (index < RADIX) {
      uint value = globalHistogram[RADIX * pass + index];
      uint excl = WavePrefixSum(value);
      uint sum = WaveActiveSum(value);

      if (WaveIsFirstLane()) {
        intermediate[waveIndex] = sum;
      }
      GroupMemoryBarrierWithGroupSync();

      if (index < RADIX / laneCount) {
        uint excl2 = WavePrefixSum(intermediate[index]);
        intermediate[index] = excl2;
      }
      GroupMemoryBarrierWithGroupSync();

      excl += intermediate[waveIndex];
      globalHistogram[RADIX * pass + index] = excl;
    }
  }
}