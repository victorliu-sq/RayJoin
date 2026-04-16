struct Intersection {
  double x;
  double y;
  uint eid0;
  uint eid1;
  int mid_point_polygon_id;
  int pad;
};

struct LaunchParams {
  int query_map_id;
  uint j;
  uint k;
  uint count;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

inline uint getQueryEid(Intersection x, int query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

inline uint getOtherEid(Intersection x, int query_map_id) {
  return (query_map_id == 0) ? x.eid1 : x.eid0;
}

bool lessXsect(Intersection a, Intersection b, int query_map_id) {
  uint aq = getQueryEid(a, query_map_id);
  uint bq = getQueryEid(b, query_map_id);

  if (aq < bq) return true;
  if (bq < aq) return false;

  // Optional deterministic tie-breakers
  uint ao = getOtherEid(a, query_map_id);
  uint bo = getOtherEid(b, query_map_id);

  if (ao < bo) return true;
  if (bo < ao) return false;

  if (a.x < b.x) return true;
  if (b.x < a.x) return false;

  if (a.y < b.y) return true;
  if (b.y < a.y) return false;

  return false;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint i = tid.x;
  if (i >= gParams.count) {
    return;
  }

  uint ixj = i ^ gParams.j;
  if (ixj <= i || ixj >= gParams.count) {
    return;
  }

  bool ascending = ((i & gParams.k) == 0);

  Intersection a = gXsects[i];
  Intersection b = gXsects[ixj];

  bool a_less_b = lessXsect(a, b, gParams.query_map_id);
  bool b_less_a = lessXsect(b, a, gParams.query_map_id);

  bool should_swap = ascending ? b_less_a : a_less_b;

  if (should_swap) {
    gXsects[i] = b;
    gXsects[ixj] = a;
  }
}