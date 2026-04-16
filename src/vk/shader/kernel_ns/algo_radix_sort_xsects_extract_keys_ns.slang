ByteAddressBuffer xsectsIn : register(t0, space0);
RWStructuredBuffer<uint> keysOut : register(u1, space0);
RWStructuredBuffer<uint> indicesOut : register(u2, space0);

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID,
          uniform uint count,
          uniform uint record_stride_words,
          uniform uint eid_word_offset,
          uniform uint _pad0) {
  const uint i = dispatchThreadID.x;
  if (i >= count) return;

  const uint key_byte_offset = (i * record_stride_words + eid_word_offset) * 4u;
  const uint key = xsectsIn.Load(key_byte_offset);

  keysOut[i] = key;
  indicesOut[i] = i;
}