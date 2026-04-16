import constants;

StructuredBuffer<uint> elementCounts : register(t0, space0);
RWStructuredBuffer<uint> globalHistogram : register(u1, space0);
RWStructuredBuffer<uint> partitionHistogram : register(u2, space0);
StructuredBuffer<uint> keysIn : register(t3, space0);
RWStructuredBuffer<uint> keysOut : register(u4, space0);
StructuredBuffer<uint> valuesIn : register(t5, space0);
RWStructuredBuffer<uint> valuesOut : register(u6, space0);

groupshared uint localHistogram[PARTITION_SIZE];
groupshared uint localHistogramSum[RADIX];

uint4 GetExclusiveWaveMask(uint id) {
  uint shift = (1 << bitfieldExtract(id, 0, 5)) - 1;
  int x = int(id) >> 5;
  return uint4((shift & ((-1 - x) >> 31)) | ((0 - x) >> 31),
               (shift & ((0 - x) >> 31)) | ((1 - x) >> 31),
               (shift & ((1 - x) >> 31)) | ((2 - x) >> 31),
               (shift & ((2 - x) >> 31)) | ((3 - x) >> 31));
}

uint GetBitCount(uint4 value) {
  uint4 result = countbits(value);
  return result[0] + result[1] + result[2] + result[3];
}

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

  uint4 waveMask = GetExclusiveWaveMask(laneIndex);

  uint partitionIndex = groupId.x;
  uint partitionStart = partitionIndex * PARTITION_SIZE;

  if (partitionStart >= elementCount) {
    return;
  }

  if (index < RADIX) {
    for (int i = 0; i < waveCount; ++i) {
      localHistogram[waveCount * index + i] = 0;
    }
  }
  GroupMemoryBarrierWithGroupSync();

  uint localKeys[PARTITION_DIVISION];
  uint localRadix[PARTITION_DIVISION];
  uint localOffsets[PARTITION_DIVISION];
  uint waveHistogram[PARTITION_DIVISION];
  uint localValues[PARTITION_DIVISION];

  [ForceUnroll]
  for (int i = 0; i < PARTITION_DIVISION; ++i) {
    uint keyIndex =
        partitionStart + (PARTITION_DIVISION * laneCount) * waveIndex + i * laneCount + laneIndex;
    uint key = keyIndex < elementCount ? keysIn[keyIndex] : 0xffffffff;
    localKeys[i] = key;
    localValues[i] = keyIndex < elementCount ? valuesIn[keyIndex] : 0;

    uint radix = bitfieldExtract(key, pass * 8, 8);
    localRadix[i] = radix;

    uint4 mask = WaveActiveBallot(true);
    [ForceUnroll]
    for (int j = 0; j < 8; ++j) {
      uint digit = (radix >> j) & 1;
      uint4 ballot = WaveActiveBallot(digit == 1);
      mask &= uint4(digit - 1) ^ ballot;
    }

    uint waveOffset = GetBitCount(waveMask & mask);
    uint radixCount = GetBitCount(mask);

    if (waveOffset == 0) {
      __atomic_add(localHistogram[waveCount * radix + waveIndex], radixCount, MemoryOrder.Relaxed);
      waveHistogram[i] = radixCount;
    } else {
      waveHistogram[i] = 0;
    }

    localOffsets[i] = waveOffset;
  }
  GroupMemoryBarrierWithGroupSync();

  for (uint i = index; i < RADIX * waveCount; i += WORKGROUP_SIZE) {
    uint v = localHistogram[i];
    uint sum = WaveActiveSum(v);
    uint excl = WavePrefixSum(v);
    localHistogram[i] = excl;
    if (laneIndex == 0) {
      localHistogramSum[i / laneCount] = sum;
    }
  }
  GroupMemoryBarrierWithGroupSync();

  uint intermediateOffset0 = RADIX * waveCount / laneCount;
  if (index < intermediateOffset0) {
    uint v = localHistogramSum[index];
    uint sum = WaveActiveSum(v);
    uint excl = WavePrefixSum(v);
    localHistogramSum[index] = excl;
    if (laneIndex == 0) {
      localHistogramSum[intermediateOffset0 + index / laneCount] = sum;
    }
  }
  GroupMemoryBarrierWithGroupSync();

  uint intermediateSize1 = max(RADIX * waveCount / laneCount / laneCount, 1);
  if (index < intermediateSize1) {
    uint v = localHistogramSum[intermediateOffset0 + index];
    uint excl = WavePrefixSum(v);
    localHistogramSum[intermediateOffset0 + index] = excl;
  }
  GroupMemoryBarrierWithGroupSync();

  if (index < intermediateOffset0) {
    localHistogramSum[index] += localHistogramSum[intermediateOffset0 + index / laneCount];
  }
  GroupMemoryBarrierWithGroupSync();

  for (uint i = index; i < RADIX * waveCount; i += WORKGROUP_SIZE) {
    localHistogram[i] += localHistogramSum[i / laneCount];
  }
  GroupMemoryBarrierWithGroupSync();

  [ForceUnroll]
  for (int i = 0; i < PARTITION_DIVISION; ++i) {
    uint radix = localRadix[i];
    localOffsets[i] += localHistogram[waveCount * radix + waveIndex];

    GroupMemoryBarrierWithGroupSync();
    if (waveHistogram[i] > 0) {
      __atomic_add(localHistogram[waveCount * radix + waveIndex], waveHistogram[i],
                   MemoryOrder.Relaxed);
    }
    GroupMemoryBarrierWithGroupSync();
  }

  if (index < RADIX) {
    uint v = index == 0 ? 0 : localHistogram[waveCount * index - 1];
    localHistogramSum[index] =
        globalHistogram[RADIX * pass + index] +
        partitionHistogram[RADIX * partitionIndex + index] - v;
  }
  GroupMemoryBarrierWithGroupSync();

  [ForceUnroll]
  for (int i = 0; i < PARTITION_DIVISION; ++i) {
    localHistogram[localOffsets[i]] = localKeys[i];
  }
  GroupMemoryBarrierWithGroupSync();

  for (uint i = index; i < PARTITION_SIZE; i += WORKGROUP_SIZE) {
    uint key = localHistogram[i];
    uint radix = bitfieldExtract(key, pass * 8, 8);
    uint dstOffset = localHistogramSum[radix] + i;
    if (dstOffset < elementCount) {
      keysOut[dstOffset] = key;
    }
    localKeys[i / WORKGROUP_SIZE] = dstOffset;
  }

  GroupMemoryBarrierWithGroupSync();

  [ForceUnroll]
  for (int i = 0; i < PARTITION_DIVISION; ++i) {
    localHistogram[localOffsets[i]] = localValues[i];
  }
  GroupMemoryBarrierWithGroupSync();

  for (uint i = index; i < PARTITION_SIZE; i += WORKGROUP_SIZE) {
    uint value = localHistogram[i];
    uint dstOffset = localKeys[i / WORKGROUP_SIZE];
    if (dstOffset < elementCount) {
      valuesOut[dstOffset] = value;
    }
  }
}