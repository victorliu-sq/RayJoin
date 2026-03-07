import scaling;

struct SrcPoint {
  double x;
  double y;
};

struct DstPoint {
  int64_t x;
  int64_t y;
};

[[vk::binding(0, 0)]]
StructuredBuffer<SrcPoint> gSrc;

[[vk::binding(1, 0)]]
RWStructuredBuffer<DstPoint> gDst;

[[vk::binding(2, 0)]]
StructuredBuffer<Scaling> gScaling;

[numthreads(256, 1, 1)]
void main(uint3 dtid: SV_DispatchThreadID) {
  uint i = dtid.x;

  Scaling sc = gScaling[0];

  double x = gSrc[i].x;
  double y = gSrc[i].y;

  gDst[i].x = sc.ScaleX(gSrc[i].x);
  gDst[i].y = sc.ScaleY(gSrc[i].y);
}