module lsi_rt;

import scaling;

// ============================================================================
// lsi_rt_debug.slang
//
// Debug-first Vulkan/Slang RT shader for proving pipeline execution.
//
// What it does:
//   - raygen increments gTestCounter[0]
//   - miss increments gTestCounter[1]
//   - intersection increments gTestCounter[2]
//   - intersection successful write path increments gTestCounter[3]
//   - raygen stores last eid in gTestCounter[4]
//   - intersection stores last prim_idx in gTestCounter[5]
//   - intersection appends a dummy record into gXsects / gXsectCounter
//
// This is intentionally NOT the final LSI logic.
// It is only for proving the RT pipeline is alive end-to-end.
//
// IMPORTANT:
//   binding 1 is StructuredBuffer<LaunchParamsLSI>, not ConstantBuffer.
//   That matches your C++ side, which binds binding 1 as STORAGE_BUFFER.
// ============================================================================

struct Edge
{
    uint p1_idx;
    uint p2_idx;
    int a;
    int b;
    int64_t c;
};

struct Point
{
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

// IMPORTANT: matches Vulkan STORAGE_BUFFER binding on C++ side
[[vk::binding(1, 0)]]
StructuredBuffer<LaunchParamsLSI> gParamsBuf;

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
// Payload
// --------------------------------------------------------------------------
struct Payload
{
    uint queryEid;
};

// ============================================================================
// Ray generation
// ============================================================================
// [shader("raygeneration")]
// void raygenMain()
// {
//     uint eid = DispatchRaysIndex().x;

//     // Count raygen invocations
//     uint old0;
//     InterlockedAdd(gTestCounter[0], 1, old0);

//     // Record last eid seen by raygen
//     gTestCounter[4] = eid;

//     LaunchParamsLSI params = gParamsBuf[0];
//     if (eid >= params.query_edge_count)
//         return;

//     // ----------------------------------------------------------------------
//     // Debug ray:
//     // Start with a simple fixed ray instead of your real edge ray.
//     // This isolates pipeline / SBT / descriptor issues from geometry issues.
//     // ----------------------------------------------------------------------
//     RayDesc ray;
//     ray.Origin = float3(0.0, 0.0, 0.0);
//     ray.Direction = float3(1.0, 0.0, 0.0);
//     ray.TMin = 0.0;
//     ray.TMax = 1e30;

//     Payload payload;
//     payload.queryEid = eid;

//     TraceRay(
//         gScene,
//         RAY_FLAG_NONE,
//         0xFF,
//         0,  // rayContributionToHitGroupIndex
//         1,  // multiplierForGeometryContributionToHitGroupIndex
//         0,  // missShaderIndex
//         ray,
//         payload);
// }

[shader("raygeneration")]
void raygenMain()
{
    uint eid = DispatchRaysIndex().x;

    // Debug counters
    uint old0;
    InterlockedAdd(gTestCounter[0], 1, old0);

    gTestCounter[4] = eid;

    LaunchParamsLSI params = gParamsBuf[0];
    if (eid >= params.query_edge_count)
        return;

    Edge e = gQueryEdges[eid];
    Point p1 = gQueryPoints[e.p1_idx];
    Point p2 = gQueryPoints[e.p2_idx];

    double x1, y1, x2, y2;

    // Match OptiX logic exactly:
    //   - vertical edge: order by y
    //   - otherwise: order by x
    if (e.b == 0)
    {
        // vertical line: p1.x == p2.x
        if (p1.y > p2.y)
        {
            Point tmp = p1;
            p1 = p2;
            p2 = tmp;
        }

        x1 = gScaling[0].UnscaleX(p1.x);
        x2 = gScaling[0].UnscaleX(p2.x);
        y1 = gScaling[0].UnscaleY(p1.y);
        y2 = gScaling[0].UnscaleY(p2.y);
    }
    else
    {
        // non-vertical line: order by x
        if (p1.x > p2.x)
        {
            Point tmp = p1;
            p1 = p2;
            p2 = tmp;
        }

        x1 = gScaling[0].UnscaleX(p1.x);
        y1 = gScaling[0].UnscaleY(p1.y);
        x2 = gScaling[0].UnscaleX(p2.x);
        y2 = gScaling[0].UnscaleY(p2.y);
    }

    RayDesc ray;
    // ray.Origin = float3((float)x1, (float)y1, 0.0);
    // ray.Direction = float3((float)(x2 - x1), (float)(y2 - y1), 0.0);
    // ray.TMin = 0.0;
    // ray.TMax = 1.0;

    ray.Origin = float3(-100.0, -3.0, 0.0);
    ray.Direction = float3(1.0, 0.0, 0.0);
    ray.TMin = 0.0;
    ray.TMax = 1000.0;

    Payload payload;
    payload.queryEid = eid;

    TraceRay(
        gScene,
        RAY_FLAG_NONE,
        0xFF,
        0,  // rayContributionToHitGroupIndex
        1,  // multiplierForGeometryContributionToHitGroupIndex
        0,  // missShaderIndex
        ray,
        payload);
}

// ============================================================================
// Miss shader
// ============================================================================
[shader("miss")]
void missMain(inout Payload payload)
{
    uint old1;
    InterlockedAdd(gTestCounter[1], 1, old1);
}

// ============================================================================
// Intersection shader
// ============================================================================
// [shader("intersection")]
// void intersectionMain(inout Payload payload)
// {
//     uint prim_idx = PrimitiveIndex();

//     // Count intersection invocations
//     uint old2;
//     InterlockedAdd(gTestCounter[2], 1, old2);

//     // Record last primitive index seen
//     gTestCounter[5] = prim_idx;

//     // Append one dummy intersection record unconditionally.
//     // This is only to prove that intersection stage can write outputs.
//     uint outIdx;
//     InterlockedAdd(gXsectCounter[0], 1, outIdx);

//     // Count successful write attempts
//     uint old3;
//     InterlockedAdd(gTestCounter[3], 1, old3);

//     LaunchParamsLSI params = gParamsBuf[0];
//     if (outIdx < params.xsect_capacity)
//     {
//         Intersection x;
//         x.x_num = 111;
//         x.x_den = 1;
//         x.y_num = 222;
//         x.y_den = 1;

//         // For debug:
//         //   eid0 = query eid from payload
//         //   eid1 = primitive index hit
//         x.eid0 = payload.queryEid;
//         x.eid1 = prim_idx;

//         x.mid_point_polygon_id = 333;
//         x._pad0 = 0;

//         gXsects[outIdx] = x;
//     }

//     // NOTE:
//     // We intentionally do NOT call ReportHit() here, because for this debug
//     // pass we only want to prove the intersection shader executes and can
//     // write to buffers.
// }

struct Rational64
{
    int64_t num;
    int64_t den;
};

bool pointEquals(Point a, Point b)
{
    return a.x == b.x && a.y == b.y;
}

int64_t min2_i64(int64_t a, int64_t b)
{
    return (a < b) ? a : b;
}

int64_t max2_i64(int64_t a, int64_t b)
{
    return (a > b) ? a : b;
}

int64_t min4_i64(int64_t a, int64_t b, int64_t c, int64_t d)
{
    return min2_i64(min2_i64(a, b), min2_i64(c, d));
}

int64_t max4_i64(int64_t a, int64_t b, int64_t c, int64_t d)
{
    return max2_i64(max2_i64(a, b), max2_i64(c, d));
}

// Normalize sign so denominator is non-negative.
// This is not strictly required for correctness of storage,
// but it makes comparison/clamping logic well-defined.
Rational64 makeRational64(int64_t num, int64_t den)
{
    Rational64 r;
    if (den < 0)
    {
        r.num = -num;
        r.den = -den;
    }
    else
    {
        r.num = num;
        r.den = den;
    }
    return r;
}

bool rationalLessThanInt(Rational64 r, int64_t v)
{
    return r.num < v * r.den;
}

bool rationalGreaterThanInt(Rational64 r, int64_t v)
{
    return r.num > v * r.den;
}

// -----------------------------------------------------------------------------
// Boolean-only intersect_test()
// -----------------------------------------------------------------------------
bool intersect_test(
    Edge e1, Point e1_p1, Point e1_p2,
    Edge e2, Point e2_p1, Point e2_p2)
{
    // SUBEDGE(p, e) = p.x * e.a + p.y * e.b + e.c
    int64_t e2_p1_agst_e1 = e2_p1.x * int64_t(e1.a) + e2_p1.y * int64_t(e1.b) + e1.c;
    int64_t e2_p2_agst_e1 = e2_p2.x * int64_t(e1.a) + e2_p2.y * int64_t(e1.b) + e1.c;
    int64_t e1_p1_agst_e2 = e1_p1.x * int64_t(e2.a) + e1_p1.y * int64_t(e2.b) + e2.c;
    int64_t e1_p2_agst_e2 = e1_p2.x * int64_t(e2.a) + e1_p2.y * int64_t(e2.b) + e2.c;

    // e1_p1 is on e2
    if (e1_p1_agst_e2 == 0)
        e1_p1_agst_e2 = -int64_t(e2.a);
    if (e1_p1_agst_e2 == 0)
        e1_p1_agst_e2 = -int64_t(e2.b);
    if (e1_p1_agst_e2 == 0)
        return false; // zero-length edge

    if (e1_p2_agst_e2 == 0)
        e1_p2_agst_e2 = -int64_t(e2.a);
    if (e1_p2_agst_e2 == 0)
        e1_p2_agst_e2 = -int64_t(e2.b);
    if (e1_p2_agst_e2 == 0)
        return false;

    // e1 endpoints on same side of e2 => no intersection
    if ((e1_p1_agst_e2 > 0 && e1_p2_agst_e2 > 0) ||
        (e1_p1_agst_e2 < 0 && e1_p2_agst_e2 < 0))
    {
        return false;
    }

    // e2_p1 is on e1
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

    // identical edges are not considered intersections
    if ((pointEquals(e1_p1, e2_p1) && pointEquals(e1_p2, e2_p2)) ||
        (pointEquals(e1_p1, e2_p2) && pointEquals(e1_p2, e2_p1)))
    {
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// intersect_test() with exact intersection point output
// -----------------------------------------------------------------------------
bool intersect_test(
    Edge e1, Point e1_p1, Point e1_p2,
    Edge e2, Point e2_p1, Point e2_p2,
    out Rational64 xsect_x,
    out Rational64 xsect_y)
{
    xsect_x = makeRational64(0, 1);
    xsect_y = makeRational64(0, 1);

    if (!intersect_test(e1, e1_p1, e1_p2, e2, e2_p1, e2_p2))
        return false;

    // calculate intersection point
    int64_t denom = int64_t(e1.a) * int64_t(e2.b) - int64_t(e2.a) * int64_t(e1.b);
    int64_t numx  = e2.c * int64_t(e1.b) - e1.c * int64_t(e2.b);
    int64_t numy  = int64_t(e2.a) * e1.c - int64_t(e1.a) * e2.c;

    xsect_x = makeRational64(numx, denom);
    xsect_y = makeRational64(numy, denom);

    int64_t t;

    t = min4_i64(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
    if (rationalLessThanInt(xsect_x, t))
        xsect_x = makeRational64(t, 1);

    t = max4_i64(e1_p1.x, e1_p2.x, e2_p1.x, e2_p2.x);
    if (rationalGreaterThanInt(xsect_x, t))
        xsect_x = makeRational64(t, 1);

    t = min4_i64(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
    if (rationalLessThanInt(xsect_y, t))
        xsect_y = makeRational64(t, 1);

    t = max4_i64(e1_p1.y, e1_p2.y, e2_p1.y, e2_p2.y);
    if (rationalGreaterThanInt(xsect_y, t))
        xsect_y = makeRational64(t, 1);

    return true;
}


[shader("intersection")]
void intersectionMain(inout Payload payload)
{
    uint prim_idx = PrimitiveIndex();
    uint query_eid = payload.queryEid;

    // Debug: count intersection-program invocations
    uint old2;
    InterlockedAdd(gTestCounter[2], 1, old2);
    gTestCounter[5] = prim_idx;

    LaunchParamsLSI params = gParamsBuf[0];
    uint query_map_id = (uint)params.query_map_id;

    Edge query_e = gQueryEdges[query_eid];
    Point query_e_p1 = gQueryPoints[query_e.p1_idx];
    Point query_e_p2 = gQueryPoints[query_e.p2_idx];

    EidRange range = gEidRanges[prim_idx];
    uint begin_eid = range.first;
    uint end_eid = range.second;

    // for (uint base_eid = begin_eid; base_eid < end_eid; ++base_eid)
    // {
    //     Edge base_e = gBaseEdges[base_eid];
    //     Point base_e_p1 = gBasePoints[base_e.p1_idx];
    //     Point base_e_p2 = gBasePoints[base_e.p2_idx];

    //     if (intersect_test(query_e, query_e_p1, query_e_p2,
    //                        base_e,  base_e_p1,  base_e_p2))
    //     {
    //         uint outIdx;
    //         InterlockedAdd(gXsectCounter[0], 1, outIdx);

    //         // Debug: count successful append attempts
    //         uint old3;
    //         InterlockedAdd(gTestCounter[3], 1, old3);

    //         if (outIdx < params.xsect_capacity)
    //         {
    //             Intersection x;
    //             x.x_num = 0;
    //             x.x_den = 1;
    //             x.y_num = 0;
    //             x.y_den = 1;

    //             // Match OptiX:
    //             // xsect.eid[query_map_id]     = query_eid;
    //             // xsect.eid[1 - query_map_id] = base_eid;
    //             if (query_map_id == 0)
    //             {
    //                 x.eid0 = query_eid;
    //                 x.eid1 = base_eid;
    //             }
    //             else
    //             {
    //                 x.eid0 = base_eid;
    //                 x.eid1 = query_eid;
    //             }

    //             x.mid_point_polygon_id = -1; // DONTKNOW equivalent
    //             x._pad0 = 0;

    //             gXsects[outIdx] = x;
    //         }
    //     }
    // }

    // Debug: count number of tested base edges for this primitive
    uint nTests = end_eid - begin_eid;
    uint old6;
    InterlockedAdd(gTestCounter[6], nTests, old6);
}