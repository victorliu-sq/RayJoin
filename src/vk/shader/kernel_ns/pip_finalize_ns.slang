module pip_finalize_ns;

struct PointD {
  double x;
  double y;
};

struct EdgeD {
  double a;
  double b;
  double c;

  uint eid;
  uint p1_idx;
  uint p2_idx;
  uint left_polygon_id;
  uint right_polygon_id;
};

struct LaunchParamsPIPFinalize {
  uint point_count;
  int exterior_face_id;
  uint _pad0;
  uint _pad1;
};

[[vk::push_constant]]
ConstantBuffer<LaunchParamsPIPFinalize> gParams;

[[vk::binding(0, 0)]]
StructuredBuffer<EdgeD> gBaseEdges;

[[vk::binding(1, 0)]]
StructuredBuffer<PointD> gBasePoints;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> gClosestEids;

[[vk::binding(3, 0)]]
RWStructuredBuffer<int> gPointInPolygon;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint idx = tid.x;
  LaunchParamsPIPFinalize params = gParams;

  if (idx >= params.point_count)
    return;

  uint eid = gClosestEids[idx];

  if (eid == 0xFFFFFFFFu) {
    gPointInPolygon[idx] = params.exterior_face_id;
    return;
  }

  EdgeD e = gBaseEdges[eid];
  PointD p1 = gBasePoints[e.p1_idx];
  PointD p2 = gBasePoints[e.p2_idx];

  int poly_id = (p1.x < p2.x) ? int(e.right_polygon_id) : int(e.left_polygon_id);
  gPointInPolygon[idx] = poly_id;
}
