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
  uint xsect_count;
  uint padded_count;
  uint _pad0;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Intersection> gXsectsIn;

[[vk::binding(1, 0)]]
RWStructuredBuffer<Intersection> gXsectsOut;

[[vk::push_constant]]
ConstantBuffer<LaunchParams> gParams;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint i = tid.x;
  if (i >= gParams.padded_count) {
    return;
  }

  if (i < gParams.xsect_count) {
    gXsectsOut[i] = gXsectsIn[i];
  } else {
    Intersection x;
    x.x = 0.0;
    x.y = 0.0;
    x.eid0 = 0xFFFFFFFFu;
    x.eid1 = 0xFFFFFFFFu;
    x.mid_point_polygon_id = -1;
    x.pad = 0;
    gXsectsOut[i] = x;
  }
}