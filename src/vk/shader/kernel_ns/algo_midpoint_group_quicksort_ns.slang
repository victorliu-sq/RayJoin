typedef uint32_t index_t;
typedef int32_t  polygon_id_t;

struct Intersection {
  double x;
  double y;
  index_t eid0;
  index_t eid1;
  polygon_id_t mid_point_polygon_id;
};

struct Point {
  double x;
  double y;
};

// Replace with your exact shader-side edge layout if needed.
// Only p1_idx is used here.
struct Edge {
  double a;
  double b;
  double c;

  index_t eid;
  index_t p1_idx;
  index_t p2_idx;
  index_t left_polygon_id;
  index_t right_polygon_id;
};

struct LaunchParams {
  int32_t query_map_id;
  uint32_t unique_count;
  uint32_t _pad0;
  uint32_t _pad1;
};

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[[vk::binding(0, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::binding(1, 0)]]
StructuredBuffer<uint32_t> gXsectIndex;

[[vk::binding(2, 0)]]
StructuredBuffer<Edge> gQueryEdges;

[[vk::binding(3, 0)]]
StructuredBuffer<Point> gQueryPoints;

index_t getQueryEid(Intersection x, int32_t query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

index_t getBaseEid(Intersection x, int32_t query_map_id) {
  return (query_map_id == 0) ? x.eid1 : x.eid0;
}

double dist2FromP1(Intersection x, Point p1) {
  double dx = x.x - p1.x;
  double dy = x.y - p1.y;
  return dx * dx + dy * dy;
}

bool lessXsect(Intersection a, Intersection b, Point p1, int32_t query_map_id) {
  double d1 = dist2FromP1(a, p1);
  double d2 = dist2FromP1(b, p1);

  if (d1 < d2) return true;
  if (d2 < d1) return false;

  return getBaseEid(a, query_map_id) < getBaseEid(b, query_map_id);
}

void swapXsect(uint32_t i, uint32_t j) {
  Intersection tmp = gXsects[i];
  gXsects[i] = gXsects[j];
  gXsects[j] = tmp;
}

uint32_t partitionRange(uint32_t lo, uint32_t hi, Point p1, int32_t query_map_id) {
  Intersection pivot = gXsects[hi];
  uint32_t i = lo;

  for (uint32_t j = lo; j < hi; ++j) {
    if (lessXsect(gXsects[j], pivot, p1, query_map_id)) {
      swapXsect(i, j);
      ++i;
    }
  }

  swapXsect(i, hi);
  return i;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint32_t group_idx = tid.x;
  if (group_idx >= gParams.unique_count) {
    return;
  }

  uint32_t begin = gXsectIndex[group_idx];
  uint32_t end = gXsectIndex[group_idx + 1u];
  uint32_t n_xsect = end - begin;

  if (n_xsect <= 1u) {
    return;
  }

  index_t eid = getQueryEid(gXsects[begin], gParams.query_map_id);
  Edge e = gQueryEdges[eid];
  Point p1 = gQueryPoints[e.p1_idx];

  // Iterative quicksort on [begin, end)
  uint2 stack[64];
  uint32_t sp = 0u;
  stack[sp++] = uint2(begin, end - 1u);

  while (sp > 0u) {
    uint2 range = stack[--sp];
    uint32_t lo = range.x;
    uint32_t hi = range.y;

    while (lo < hi) {
      uint32_t p = partitionRange(lo, hi, p1, gParams.query_map_id);

      uint32_t left_size = (p > lo) ? (p - lo) : 0u;
      uint32_t right_size = (hi > p) ? (hi - p) : 0u;

      // Push larger side, continue with smaller side to keep stack bounded
      if (left_size > right_size) {
        if (left_size > 1u) {
          stack[sp++] = uint2(lo, p - 1u);
        }
        lo = p + 1u;
      } else {
        if (right_size > 1u) {
          stack[sp++] = uint2(p + 1u, hi);
        }
        if (p == 0u) {
          break;
        }
        hi = p - 1u;
      }
    }
  }
}