module lsi_rt;

import scaling;

// ============================================================================
// lsi_rt.slang
//
// Vulkan/Slang port of the OptiX LSI shader.
//
// Entry points:
//   - raygenMain
//   - missMain
//   - intersectionMain
//
// Descriptor bindings expected:
//
//   binding 0 : RaytracingAccelerationStructure gScene
//   binding 1 : ConstantBuffer<LaunchParamsLSI> gParams
//   binding 2 : StructuredBuffer<Edge>          gBaseEdges
//   binding 3 : StructuredBuffer<Point>         gBasePoints
//   binding 4 : StructuredBuffer<EidRange>      gEidRanges
//   binding 5 : StructuredBuffer<Edge>          gQueryEdges
//   binding 6 : StructuredBuffer<Point>         gQueryPoints
//   binding 7 : RWStructuredBuffer<Intersection> gXsects
//   binding 8 : RWStructuredBuffer<uint>         gXsectCounter
//   binding 9 : RWStructuredBuffer<uint>         gTestCounter
//   binding 10: StructuredBuffer<Scaling>        gScaling
//
// Notes:
// - This shader mirrors the OptiX logic where the RT pass only records
//   (query_eid, base_eid). Exact x/y can still be resolved later in a
//   separate compute pass, as your OptiX implementation does.
// - Replace Edge / Point / Intersection layouts if needed to exactly match C++.
// ============================================================================

struct Edge
{
    // Match your C++ Edge layout
    uint p1_idx;
    uint p2_idx;
    int a;
    int b;
    int64_t c;
};

struct Point
{
    // Match your C++ point layout
    int64_t x;
    int64_t y;
};

struct EidRange
{
    uint first;
    uint second;
};

struct Intersection
{
    // Placeholder exact-point fields. Keep layout aligned with C++ Intersection.
    int64_t x_num;
    int64_t x_den;
    int64_t y_num;
    int64_t y_den;

    uint eid0;
    uint eid1;

    int mid_point_polygon_id;
    int _pad0;
};

struct LaunchParamsLSI
{
    int query_map_id;
    uint query_edge_count;
    uint xsect_capacity;
    uint _pad0;
};

// --------------------------------------------------------------------------
// Bindings
// --------------------------------------------------------------------------
[[vk::binding(0, 0)]]
RaytracingAccelerationStructure gScene;

[[vk::binding(1, 0)]]
ConstantBuffer<LaunchParamsLSI> gParams;

[[vk::binding(2, 0)]]
StructuredBuffer<Edge> gBaseEdges;

[[vk::binding(3, 0)]]
StructuredBuffer<Point> gBasePoints;

[[vk::binding(4, 0)]]
StructuredBuffer<EidRange> gEidRanges;

[[vk::binding(5, 0)]]
StructuredBuffer<Edge> gQueryEdges;

[[vk::binding(6, 0)]]
StructuredBuffer<Point> gQueryPoints;

[[vk::binding(7, 0)]]
RWStructuredBuffer<Intersection> gXsects;

[[vk::binding(8, 0)]]
RWStructuredBuffer<uint> gXsectCounter;

[[vk::binding(9, 0)]]
RWStructuredBuffer<uint> gTestCounter;

[[vk::binding(10, 0)]]
StructuredBuffer<Scaling> gScaling;

// --------------------------------------------------------------------------
// Payload: carries query edge id from raygen to intersection.
// --------------------------------------------------------------------------
struct Payload
{
    uint queryEid;
};

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------
static int64_t subedge(Point p, Edge e)
{
    // ax + by + c
    return p.x * int64_t(e.a) + p.y * int64_t(e.b) + e.c;
}

static bool pointEqual(Point a, Point b)
{
    return (a.x == b.x) && (a.y == b.y);
}

// Port of your C++ intersect_test(...)
static bool intersect_test(Edge e1, Point e1_p1, Point e1_p2,
                           Edge e2, Point e2_p1, Point e2_p2)
{
    int64_t e2_p1_agst_e1 = subedge(e2_p1, e1);
    int64_t e2_p2_agst_e1 = subedge(e2_p2, e1);
    int64_t e1_p1_agst_e2 = subedge(e1_p1, e2);
    int64_t e1_p2_agst_e2 = subedge(e1_p2, e2);

    if (e1_p1_agst_e2 == 0)
        e1_p1_agst_e2 = -int64_t(e2.a);
    if (e1_p1_agst_e2 == 0)
        e1_p1_agst_e2 = -int64_t(e2.b);
    if (e1_p1_agst_e2 == 0)
        return false;

    if (e1_p2_agst_e2 == 0)
        e1_p2_agst_e2 = -int64_t(e2.a);
    if (e1_p2_agst_e2 == 0)
        e1_p2_agst_e2 = -int64_t(e2.b);
    if (e1_p2_agst_e2 == 0)
        return false;

    if ((e1_p1_agst_e2 > 0 && e1_p2_agst_e2 > 0) ||
        (e1_p1_agst_e2 < 0 && e1_p2_agst_e2 < 0))
    {
        return false;
    }

    if (e2_p1_agst_e1 == 0)
        e2_p1_agst_e1 = int64_t(e1.a);
    if (e2_p1_agst_e1 == 0)
        e2_p1_agst_e1 = int64_t(e1.b);
    if (e2_p1_agst_e1 == 0)
        return false;

    if (e2_p2_agst_e1 == 0)
        e2_p2_agst_e1 = int64_t(e1.a);
    if (e2_p2_agst_e1 == 0)
        e2_p2_agst_e1 = int64_t(e1.b);
    if (e2_p2_agst_e1 == 0)
        return false;

    if ((e2_p1_agst_e1 > 0 && e2_p2_agst_e1 > 0) ||
        (e2_p1_agst_e1 < 0 && e2_p2_agst_e1 < 0))
    {
        return false;
    }

    if ((pointEqual(e1_p1, e2_p1) && pointEqual(e1_p2, e2_p2)) ||
        (pointEqual(e1_p1, e2_p2) && pointEqual(e1_p2, e2_p1)))
    {
        return false;
    }

    return true;
}

// ============================================================================
// Ray generation
//
// OptiX equivalent:
//   __raygen__lsi_custom()
// ============================================================================
[shader("raygeneration")]
void raygenMain()
{
    uint eid = DispatchRaysIndex().x;
    if (eid >= gParams.query_edge_count)
        return;

    Scaling sc = gScaling[0];

    Edge e = gQueryEdges[eid];
    Point p1 = gQueryPoints[e.p1_idx];
    Point p2 = gQueryPoints[e.p2_idx];

    double x1, x2, y1, y2;

    // Match OptiX ordering logic exactly
    if (e.b == 0)
    {
        // vertical edge
        if (p1.y > p2.y)
        {
            Point tmp = p1;
            p1 = p2;
            p2 = tmp;
        }

        x1 = sc.UnscaleX(p1.x);
        x2 = sc.UnscaleX(p2.x);
        y1 = sc.UnscaleY(p1.y);
        y2 = sc.UnscaleY(p2.y);
    }
    else
    {
        // non-vertical: order by x
        if (p1.x > p2.x)
        {
            Point tmp = p1;
            p1 = p2;
            p2 = tmp;
        }

        x1 = sc.UnscaleX(p1.x);
        y1 = sc.UnscaleY(p1.y);
        x2 = sc.UnscaleX(p2.x);
        y2 = sc.UnscaleY(p2.y);
    }

    RayDesc ray;
    ray.Origin = float3((float)x1, (float)y1, 0.0);
    ray.Direction = float3((float)(x2 - x1), (float)(y2 - y1), 0.0);
    ray.TMin = 0.0;
    ray.TMax = 1.0;

    Payload payload;
    payload.queryEid = eid;

    TraceRay(
        gScene,
        RAY_FLAG_NONE,
        0xFF,
        0, // rayContributionToHitGroupIndex
        1, // multiplierForGeometryContributionToHitGroupIndex
        0, // missShaderIndex
        ray,
        payload);
}

// ============================================================================
// Miss shader
// ============================================================================
[shader("miss")]
void missMain(inout Payload payload)
{
    // No-op
}

// ============================================================================
// Intersection shader
//
// OptiX equivalent:
//   __intersection__lsi_custom()
// ============================================================================
[shader("intersection")]
void intersectionMain(inout Payload payload)
{
    uint prim_idx = PrimitiveIndex();
    uint query_eid = payload.queryEid;
    int query_map_id = gParams.query_map_id;

    Edge query_e = gQueryEdges[query_eid];
    Point query_p1 = gQueryPoints[query_e.p1_idx];
    Point query_p2 = gQueryPoints[query_e.p2_idx];

    EidRange range = gEidRanges[prim_idx];
    uint begin_eid = range.first;
    uint end_eid = range.second;

    for (uint base_eid = begin_eid; base_eid < end_eid; ++base_eid)
    {
        Edge base_e = gBaseEdges[base_eid];
        Point base_p1 = gBasePoints[base_e.p1_idx];
        Point base_p2 = gBasePoints[base_e.p2_idx];

        if (intersect_test(query_e, query_p1, query_p2,
                           base_e, base_p1, base_p2))
        {
            uint outIdx;
            InterlockedAdd(gXsectCounter[0], 1, outIdx);

            if (outIdx < gParams.xsect_capacity)
            {
                Intersection x;
                x.x_num = 0;
                x.x_den = 1;
                x.y_num = 0;
                x.y_den = 1;

                // Match OptiX:
                // xsect.eid[query_map_id] = query_eid;
                // xsect.eid[1 - query_map_id] = base_eid;
                if (query_map_id == 0)
                {
                    x.eid0 = query_eid;
                    x.eid1 = base_eid;
                }
                else
                {
                    x.eid0 = base_eid;
                    x.eid1 = query_eid;
                }

                // DONTKNOW placeholder
                x.mid_point_polygon_id = -1;
                x._pad0 = 0;

                gXsects[outIdx] = x;
            }
        }
    }

    uint nTests = end_eid - begin_eid;
    uint oldValue;
    InterlockedAdd(gTestCounter[0], nTests, oldValue);
}
