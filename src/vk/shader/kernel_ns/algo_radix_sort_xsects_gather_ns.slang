ByteAddressBuffer xsectsIn : register(t0, space0);
StructuredBuffer<uint> sortedIndices : register(t1, space0);
RWByteAddressBuffer xsectsOut : register(u2, space0);

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID,
          uniform uint count,
          uniform uint record_stride_words,
          uniform uint _pad0,
          uniform uint _pad1) {
  uint dstIndex = dispatchThreadID.x;
  if (dstIndex >= count) return;

  uint srcIndex = sortedIndices[dstIndex];

  uint srcBaseWord = srcIndex * record_stride_words;
  uint dstBaseWord = dstIndex * record_stride_words;

  for (uint w = 0; w < record_stride_words; ++w) {
    uint v = xsectsIn.Load((srcBaseWord + w) * 4u);
    xsectsOut.Store((dstBaseWord + w) * 4u, v);
  }
}