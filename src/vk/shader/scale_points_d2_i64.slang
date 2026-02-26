// scale_points_d2_i64.slang
// Vulkan compute shader via Slang -> SPIR-V
struct SrcPoint {
  double x;
  double y;
};

// std430 rules mean double2 is 16-byte aligned; struct is 16 bytes.
struct DstPoint {
  int64_t x;
  int64_t y;
};

StructuredBuffer<SrcPoint> gSrc : register(t0);    // binding 0
RWStructuredBuffer<DstPoint> gDst : register(u1);  // binding 1

struct PushConstants {
  double rx;
  double ry;
  double deltax;
  double deltay;
  uint count;
  uint _pad0;  // keep 8-byte alignment clean
};

[[vk::push_constant]]
PushConstants pc;

[numthreads(256, 1, 1)]
void main(uint3 dtid: SV_DispatchThreadID) {
  uint i = dtid.x;
  if (i >= pc.count)
    return;

  double x = gSrc[i].x;
  double y = gSrc[i].y;

  // Match C++ semantics: cast truncates toward zero
  int64_t sx = (int64_t) (x * pc.rx + pc.deltax);
  int64_t sy = (int64_t) (y * pc.ry + pc.deltay);

  gDst[i].x = sx;
  gDst[i].y = sy;
}
