struct Intersection {
  double x;
  double y;
  uint eid0;
  uint eid1;
  int mid_point_polygon_id;
  int pad;
};

struct XsectSortEntry {
  uint query_eid;
  uint src_idx;
  uint _pad0;
  uint _pad1;
};

struct LaunchParams {
  int32_t query_map_id;
  uint32_t xsect_count;
  uint32_t padded_count;
  uint32_t _pad0;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Intersection> gXsectsIn;

[[vk::binding(1, 0)]]
RWStructuredBuffer<Intersection> gXsectsOut;

[[vk::binding(2, 0)]]
RWStructuredBuffer<XsectSortEntry> gSortEntries;

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

uint getQueryEid(Intersection x, int32_t query_map_id) {
  return (query_map_id == 0) ? x.eid0 : x.eid1;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid: SV_DispatchThreadID) {
  uint32_t i = tid.x;
  if (i >= gParams.padded_count) {
    return;
  }

  if (i < gParams.xsect_count) {
    Intersection in_x = gXsectsIn[i];

    Intersection out_x;
    out_x.x = in_x.x;
    out_x.y = in_x.y;
    out_x.eid0 = in_x.eid0;
    out_x.eid1 = in_x.eid1;
    out_x.mid_point_polygon_id = in_x.mid_point_polygon_id;
    out_x.pad = in_x.pad;
    gXsectsOut[i] = out_x;

    XsectSortEntry e;
    e.query_eid = getQueryEid(in_x, gParams.query_map_id);
    e.src_idx = i;
    e._pad0 = 0u;
    e._pad1 = 0u;
    gSortEntries[i] = e;
  } else {
    Intersection out_x;
    out_x.x = 0.0;
    out_x.y = 0.0;
    out_x.eid0 = 0xFFFFFFFFu;
    out_x.eid1 = 0xFFFFFFFFu;
    out_x.mid_point_polygon_id = -1;
    out_x.pad = 0;
    gXsectsOut[i] = out_x;

    XsectSortEntry e;
    e.query_eid = 0xFFFFFFFFu;
    e.src_idx = 0xFFFFFFFFu;
    e._pad0 = 0u;
    e._pad1 = 0u;
    gSortEntries[i] = e;
  }
}
