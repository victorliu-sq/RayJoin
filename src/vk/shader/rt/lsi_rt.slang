import lsi_common;

struct LaunchParamsLSI {
  int query_map_id;
  uint query_edge_count;
  uint xsect_capacity;
  uint pad0;
};

struct Payload {
  uint queryEid;
};

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure gScene;

[[vk::push_constant]]
ConstantBuffer<LaunchParamsLSI> gParams;

[[vk::binding(1, 0)]]
StructuredBuffer<Edge> gBaseEdges;

[[vk::binding(2, 0)]]
StructuredBuffer<Point> gBasePoints;

[[vk::binding(3, 0)]]
StructuredBuffer<EidRange> gEidRanges;

[[vk::binding(4, 0)]]
StructuredBuffer<Edge> gQueryEdges;

[[vk::binding(5, 0)]]
StructuredBuffer<Point> gQueryPoints;

[[vk::binding(6, 0)]]
StructuredBuffer<Scaling> gQueryScaling;

[[vk::binding(7, 0)]]
RWStructuredBuffer<Intersection128> gXsects;

[[vk::binding(8, 0)]]
RWStructuredBuffer<uint> gXsectCounter;

[[vk::binding(9, 0)]]
RWStructuredBuffer<uint> gTestCounter;

[shader("raygeneration")]
void raygenMain() {
  uint eid = DispatchRaysIndex().x;
  LaunchParamsLSI params = gParams;

  if (eid >= params.query_edge_count)
    return;

  Edge e = gQueryEdges[eid];
  Point p1 = gQueryPoints[e.p1_idx];
  Point p2 = gQueryPoints[e.p2_idx];
  Scaling sc = gQueryScaling[0];

  if (i128_is_zero(e.b)) {
    if (p1.y > p2.y) {
      Point tmp = p1;
      p1 = p2;
      p2 = tmp;
    }
  } else {
    if (p1.x > p2.x) {
      Point tmp = p1;
      p1 = p2;
      p2 = tmp;
    }
  }

  double x1 = sc.UnscaleX(p1.x);
  double y1 = sc.UnscaleY(p1.y);
  double x2 = sc.UnscaleX(p2.x);
  double y2 = sc.UnscaleY(p2.y);

  RayDesc ray;
  ray.Origin = float3(float(x1), float(y1), 0.0f);
  ray.Direction = float3(float(x2 - x1), float(y2 - y1), 0.0f);
  ray.TMin = 0.0f;
  ray.TMax = 1.0f;

  Payload payload;
  payload.queryEid = eid;

  TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
}

[shader("miss")]
void missMain(inout Payload payload) {}

[shader("intersection")]
void intersectionMain() {
  uint prim_idx = PrimitiveIndex();

  uint3 ray_idx = DispatchRaysIndex();
  uint3 ray_dim = DispatchRaysDimensions();
  uint query_eid = ray_idx.x + ray_dim.x * (ray_idx.y + ray_dim.y * ray_idx.z);

  LaunchParamsLSI params = gParams;
  uint query_map_id = (uint)params.query_map_id;

  Edge query_e = gQueryEdges[query_eid];
  Point query_e_p1 = gQueryPoints[query_e.p1_idx];
  Point query_e_p2 = gQueryPoints[query_e.p2_idx];

  EidRange range = gEidRanges[prim_idx];
  uint begin_eid = range.eid1;
  uint end_eid = range.eid2;

  for (uint base_eid = begin_eid; base_eid < end_eid; ++base_eid) {
    Edge base_e = gBaseEdges[base_eid];
    Point base_e_p1 = gBasePoints[base_e.p1_idx];
    Point base_e_p2 = gBasePoints[base_e.p2_idx];

    if (intersect_test(query_e, query_e_p1, query_e_p2, base_e, base_e_p1, base_e_p2)) {
      uint outIdx;
      InterlockedAdd(gXsectCounter[0], 1, outIdx);

      if (outIdx < params.xsect_capacity) {
        Intersection128 x;
        x.x.num = make_i128(0, 0);
        x.x.den = make_i128(0, 1);
        x.y.num = make_i128(0, 0);
        x.y.den = make_i128(0, 1);

        if (query_map_id == 0) {
          x.eid0 = query_eid;
          x.eid1 = base_eid;
        } else {
          x.eid0 = base_eid;
          x.eid1 = query_eid;
        }

        x.mid_point_polygon_id = -1;
        x.pad = 0u;
        gXsects[outIdx] = x;
      }
    }
  }
}

[shader("anyhit")]
void anyHitMain(inout Payload payload) {}

[shader("closesthit")]
void closestHitMain(inout Payload payload) {}
