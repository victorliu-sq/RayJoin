module lsi_common;

public struct Point {
  public int64_t x;
  public int64_t y;
};

public struct Int128 {
  public uint64_t lo;
  public int64_t hi;
};

public struct Edge {
  public Int128 a;
  public Int128 b;
  public Int128 c;

  public uint eid;
  public uint p1_idx;
  public uint p2_idx;
  public uint left_polygon_id;
  public uint right_polygon_id;

  public uint pad0;
  public uint pad1;
  public uint pad2;
};

public struct EidRange {
  public uint eid1;
  public uint eid2;
};

public struct Rational128 {
  public Int128 num;
  public Int128 den;
};

public struct Intersection128 {
  public Rational128 x;
  public Rational128 y;
  public uint eid0;
  public uint eid1;
  public int mid_point_polygon_id;
  public uint pad;
};

public struct Scaling {
  public int64_t internal_max;
  public int64_t internal_min;
  public int64_t internal_range;

  public double rx;
  public double ry;
  public double rrx;
  public double rry;

  public double deltax;
  public double deltay;

  public double ddeltax;
  public double ddeltay;

  public int64_t ScaleX(double x) {
    return (int64_t)(x * rx + deltax);
  }

  public int64_t ScaleY(double y) {
    return (int64_t)(y * ry + deltay);
  }

  public double UnscaleX(int64_t ix) {
    return ix * rrx + ddeltax;
  }

  public double UnscaleY(int64_t iy) {
    return iy * rry + ddeltay;
  }
}

public Int128 make_i128(int64_t hi, uint64_t lo) {
  Int128 v;
  v.lo = lo;
  v.hi = hi;
  return v;
}

public Int128 i128_from_i64(int64_t x) {
  return make_i128((x < 0) ? -1ll : 0ll, (uint64_t)x);
}

public bool i128_is_zero(Int128 x) {
  return x.lo == 0ull && x.hi == 0ll;
}

public bool i128_is_neg(Int128 x) {
  return x.hi < 0;
}

public Int128 i128_not(Int128 x) {
  return make_i128(~x.hi, ~x.lo);
}

public Int128 i128_add(Int128 a, Int128 b) {
  Int128 r;
  r.lo = a.lo + b.lo;
  uint64_t carry = (r.lo < a.lo) ? 1ull : 0ull;
  r.hi = a.hi + b.hi + (int64_t)carry;
  return r;
}

public Int128 i128_neg(Int128 x) {
  return i128_add(i128_not(x), make_i128(0ll, 1ull));
}

public Int128 i128_sub(Int128 a, Int128 b) {
  return i128_add(a, i128_neg(b));
}

public bool i128_eq(Int128 a, Int128 b) {
  return a.lo == b.lo && a.hi == b.hi;
}

public bool i128_lt(Int128 a, Int128 b) {
  if (a.hi != b.hi)
    return a.hi < b.hi;
  return a.lo < b.lo;
}

public bool i128_gt(Int128 a, Int128 b) {
  if (a.hi != b.hi)
    return a.hi > b.hi;
  return a.lo > b.lo;
}

public Int128 i128_abs(Int128 x) {
  return i128_is_neg(x) ? i128_neg(x) : x;
}

public int64_t i128_to_i64_lossless(Int128 x) {
  return (int64_t)x.lo;
}

public Int128 mul_u64_u64(uint64_t x, uint64_t y) {
  uint64_t x0 = x & 0xffffffffull;
  uint64_t x1 = x >> 32;
  uint64_t y0 = y & 0xffffffffull;
  uint64_t y1 = y >> 32;

  uint64_t p00 = x0 * y0;
  uint64_t p01 = x0 * y1;
  uint64_t p10 = x1 * y0;
  uint64_t p11 = x1 * y1;

  uint64_t middle = (p00 >> 32) + (p01 & 0xffffffffull) + (p10 & 0xffffffffull);
  uint64_t lo = (p00 & 0xffffffffull) | (middle << 32);
  uint64_t hi = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);

  return make_i128((int64_t)hi, lo);
}

public Int128 mul_i64_i64(int64_t a, int64_t b) {
  bool neg = ((a < 0) != (b < 0));
  uint64_t ua = (a < 0) ? (uint64_t)(-a) : (uint64_t)a;
  uint64_t ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;
  Int128 r = mul_u64_u64(ua, ub);
  return neg ? i128_neg(r) : r;
}

public Int128 mul_i128_i64(Int128 a, int64_t b) {
  bool neg = (i128_is_neg(a) != (b < 0));
  Int128 aa = i128_abs(a);
  uint64_t ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;

  Int128 lo_prod = mul_u64_u64(aa.lo, ub);
  Int128 hi_prod = mul_u64_u64((uint64_t)aa.hi, ub);

  Int128 r;
  r.lo = lo_prod.lo;
  r.hi = (int64_t)((uint64_t)lo_prod.hi + hi_prod.lo);

  return neg ? i128_neg(r) : r;
}

public Int128 subedge(Point p, Edge e) {
  int64_t a64 = i128_to_i64_lossless(e.a);
  int64_t b64 = i128_to_i64_lossless(e.b);

  return i128_add(i128_add(mul_i64_i64(p.x, a64), mul_i64_i64(p.y, b64)), e.c);
}

public Int128 tie_break_neg(Edge e) {
  if (!i128_is_zero(e.a))
    return i128_neg(e.a);
  if (!i128_is_zero(e.b))
    return i128_neg(e.b);
  return make_i128(0, 0);
}

public Int128 tie_break_pos(Edge e) {
  if (!i128_is_zero(e.a))
    return e.a;
  if (!i128_is_zero(e.b))
    return e.b;
  return make_i128(0, 0);
}

public bool point_equals(Point a, Point b) {
  return a.x == b.x && a.y == b.y;
}

public bool same_sign_nonzero(Int128 a, Int128 b) {
  if (i128_is_zero(a) || i128_is_zero(b))
    return false;
  return (i128_is_neg(a) && i128_is_neg(b)) || (!i128_is_neg(a) && !i128_is_neg(b));
}

public bool intersect_test(Edge e1, Point e1_p1, Point e1_p2, Edge e2, Point e2_p1, Point e2_p2) {
  Int128 e2_p1_agst_e1 = subedge(e2_p1, e1);
  Int128 e2_p2_agst_e1 = subedge(e2_p2, e1);
  Int128 e1_p1_agst_e2 = subedge(e1_p1, e2);
  Int128 e1_p2_agst_e2 = subedge(e1_p2, e2);

  if (i128_is_zero(e1_p1_agst_e2))
    e1_p1_agst_e2 = tie_break_neg(e2);
  if (i128_is_zero(e1_p1_agst_e2))
    return false;

  if (i128_is_zero(e1_p2_agst_e2))
    e1_p2_agst_e2 = tie_break_neg(e2);
  if (i128_is_zero(e1_p2_agst_e2))
    return false;

  if (same_sign_nonzero(e1_p1_agst_e2, e1_p2_agst_e2))
    return false;

  if (i128_is_zero(e2_p1_agst_e1))
    e2_p1_agst_e1 = tie_break_pos(e1);
  if (i128_is_zero(e2_p1_agst_e1))
    return false;

  if (i128_is_zero(e2_p2_agst_e1))
    e2_p2_agst_e1 = tie_break_pos(e1);
  if (i128_is_zero(e2_p2_agst_e1))
    return false;

  if (same_sign_nonzero(e2_p1_agst_e1, e2_p2_agst_e1))
    return false;

  if ((point_equals(e1_p1, e2_p1) && point_equals(e1_p2, e2_p2)) ||
      (point_equals(e1_p1, e2_p2) && point_equals(e1_p2, e2_p1))) {
    return false;
  }

  return true;
}
