// // scale_points_d2_i64.slang
// // Vulkan compute shader via Slang -> SPIR-V
// struct SrcPoint {
//   double x;
//   double y;
// };

// // std430 rules mean double2 is 16-byte aligned; struct is 16 bytes.
// struct DstPoint {
//   int64_t x;
//   int64_t y;
// };

// // StructuredBuffer<SrcPoint> gSrc : register(t0);    // binding 0
// // RWStructuredBuffer<DstPoint> gDst : register(u1);  // binding 1
// [[vk::binding(0, 0)]]
// StructuredBuffer<SrcPoint> gSrc;
// [[vk::binding(1, 0)]]
// RWStructuredBuffer<DstPoint> gDst;

// struct PushConstants {
//   double rx;
//   double ry;
//   double deltax;
//   double deltay;
//   uint ptr_count;
//   uint _pad0;  // keep 8-byte alignment clean
// };

// [[vk::push_constant]]
// PushConstants pc;

// [numthreads(256, 1, 1)]
// void main(uint3 dtid: SV_DispatchThreadID) {
//   uint i = dtid.x;
//   if (i >= pc.ptr_count)
//     return;

//   double x = gSrc[i].x;
//   double y = gSrc[i].y;

//   // Match C++ semantics: cast truncates toward zero
//   int64_t sx = (int64_t) (x * pc.rx + pc.deltax);
//   int64_t sy = (int64_t) (y * pc.ry + pc.deltay);

//   gDst[i].x = sx;
//   gDst[i].y = sy;
// }

struct SrcPoint {
  double x;
  double y;
};

struct DstPoint {
  int64_t x;
  int64_t y;
};

struct Scaling {
  int64_t internal_max;
  int64_t internal_min;
  int64_t internal_range;

  double rx;
  double ry;
  double rrx;
  double rry;

  double deltax;
  double deltay;

  double ddeltax;
  double ddeltay;

  inline int64_t ScaleX(double x) {
    return (int64_t) (x * rx + deltax);
  }

  inline int64_t ScaleY(double y) {
    return (int64_t) (y * ry + deltay);
  }

  inline double UnscaleX(int64_t internal_x) {
    return internal_x * rrx + ddeltax;
  }

  inline double UnscaleY(int64_t internal_y) {
    return internal_y * rry + ddeltay;
  }
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
