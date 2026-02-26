#ifndef RAYJOIN_GPU_EDGE_TYPES_H
#define RAYJOIN_GPU_EDGE_TYPES_H

#include <cstdint>

struct alignas(16) GpuPointI64 {
  int64_t x;
  int64_t y;
};
static_assert(sizeof(GpuPointI64) == 16);

struct alignas(8) GpuChain {
  int32_t left_polygon_id;
  int32_t right_polygon_id;
};
static_assert(sizeof(GpuChain) == 8);

using GpuIndex = uint32_t;

struct alignas(16) GpuEdge {
  int64_t a;
  int64_t b;
  int64_t c;

  uint32_t eid;
  uint32_t p1_idx;
  uint32_t p2_idx;
  uint32_t left_polygon_id;
  uint32_t right_polygon_id;
};
static_assert(sizeof(GpuEdge) == 48); // 3*8 + 5*4 = 48

#endif  // RAYJOIN_GPU_EDGE_TYPES_H
