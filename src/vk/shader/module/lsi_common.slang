// module lsi_common;

// public struct Point {
//   public int64_t x;
//   public int64_t y;
// };

// public struct Int128 {
//   public uint64_t lo;
//   public int64_t hi;
// };

// public struct Edge {
//   public Int128 a;
//   public Int128 b;
//   public Int128 c;

//   public uint eid;
//   public uint p1_idx;
//   public uint p2_idx;
//   public uint left_polygon_id;
//   public uint right_polygon_id;

//   public uint pad0;
//   public uint pad1;
//   public uint pad2;
// };

// public struct EidRange {
//   public uint eid1;
//   public uint eid2;
// };

// public struct Rational128 {
//   public Int128 num;
//   public Int128 den;
// };

// public struct Intersection128 {
//   public Rational128 x;
//   public Rational128 y;
//   public uint eid0;
//   public uint eid1;
//   public int mid_point_polygon_id;
//   public uint pad;
// };

// public struct Scaling {
//   public int64_t internal_max;
//   public int64_t internal_min;
//   public int64_t internal_range;

//   public double rx;
//   public double ry;
//   public double rrx;
//   public double rry;

//   public double deltax;
//   public double deltay;

//   public double ddeltax;
//   public double ddeltay;

//   public int64_t ScaleX(double x) {
//     return (int64_t)(x * rx + deltax);
//   }

//   public int64_t ScaleY(double y) {
//     return (int64_t)(y * ry + deltay);
//   }

//   public double UnscaleX(int64_t ix) {
//     return ix * rrx + ddeltax;
//   }

//   public double UnscaleY(int64_t iy) {
//     return iy * rry + ddeltay;
//   }
// }

// public Int128 make_i128(int64_t hi, uint64_t lo) {
//   Int128 v;
//   v.lo = lo;
//   v.hi = hi;
//   return v;
// }

// public Int128 i128_from_i64(int64_t x) {
//   return make_i128((x < 0) ? -1ll : 0ll, (uint64_t)x);
// }

// public bool i128_is_zero(Int128 x) {
//   return x.lo == 0ull && x.hi == 0ll;
// }

// public bool i128_is_neg(Int128 x) {
//   return x.hi < 0;
// }

// public Int128 i128_not(Int128 x) {
//   return make_i128(~x.hi, ~x.lo);
// }

// public Int128 i128_add(Int128 a, Int128 b) {
//   Int128 r;
//   r.lo = a.lo + b.lo;
//   uint64_t carry = (r.lo < a.lo) ? 1ull : 0ull;
//   r.hi = a.hi + b.hi + (int64_t)carry;
//   return r;
// }

// public Int128 i128_neg(Int128 x) {
//   return i128_add(i128_not(x), make_i128(0ll, 1ull));
// }

// public Int128 i128_sub(Int128 a, Int128 b) {
//   return i128_add(a, i128_neg(b));
// }

// public bool i128_eq(Int128 a, Int128 b) {
//   return a.lo == b.lo && a.hi == b.hi;
// }

// public bool i128_lt(Int128 a, Int128 b) {
//   if (a.hi != b.hi)
//     return a.hi < b.hi;
//   return a.lo < b.lo;
// }

// public bool i128_gt(Int128 a, Int128 b) {
//   if (a.hi != b.hi)
//     return a.hi > b.hi;
//   return a.lo > b.lo;
// }

// public Int128 i128_abs(Int128 x) {
//   return i128_is_neg(x) ? i128_neg(x) : x;
// }

// public int64_t i128_to_i64_lossless(Int128 x) {
//   return (int64_t)x.lo;
// }

// public Int128 mul_u64_u64(uint64_t x, uint64_t y) {
//   uint64_t x0 = x & 0xffffffffull;
//   uint64_t x1 = x >> 32;
//   uint64_t y0 = y & 0xffffffffull;
//   uint64_t y1 = y >> 32;

//   uint64_t p00 = x0 * y0;
//   uint64_t p01 = x0 * y1;
//   uint64_t p10 = x1 * y0;
//   uint64_t p11 = x1 * y1;

//   uint64_t middle = (p00 >> 32) + (p01 & 0xffffffffull) + (p10 & 0xffffffffull);
//   uint64_t lo = (p00 & 0xffffffffull) | (middle << 32);
//   uint64_t hi = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);

//   return make_i128((int64_t)hi, lo);
// }

// public Int128 mul_i64_i64(int64_t a, int64_t b) {
//   bool neg = ((a < 0) != (b < 0));
//   uint64_t ua = (a < 0) ? (uint64_t)(-a) : (uint64_t)a;
//   uint64_t ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;
//   Int128 r = mul_u64_u64(ua, ub);
//   return neg ? i128_neg(r) : r;
// }

// public Int128 mul_i128_i64(Int128 a, int64_t b) {
//   bool neg = (i128_is_neg(a) != (b < 0));
//   Int128 aa = i128_abs(a);
//   uint64_t ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;

//   Int128 lo_prod = mul_u64_u64(aa.lo, ub);
//   Int128 hi_prod = mul_u64_u64((uint64_t)aa.hi, ub);

//   Int128 r;
//   r.lo = lo_prod.lo;
//   r.hi = (int64_t)((uint64_t)lo_prod.hi + hi_prod.lo);

//   return neg ? i128_neg(r) : r;
// }

// public Int128 subedge(Point p, Edge e) {
//   int64_t a64 = i128_to_i64_lossless(e.a);
//   int64_t b64 = i128_to_i64_lossless(e.b);

//   return i128_add(i128_add(mul_i64_i64(p.x, a64), mul_i64_i64(p.y, b64)), e.c);
// }

// public Int128 tie_break_neg(Edge e) {
//   if (!i128_is_zero(e.a))
//     return i128_neg(e.a);
//   if (!i128_is_zero(e.b))
//     return i128_neg(e.b);
//   return make_i128(0, 0);
// }

// public Int128 tie_break_pos(Edge e) {
//   if (!i128_is_zero(e.a))
//     return e.a;
//   if (!i128_is_zero(e.b))
//     return e.b;
//   return make_i128(0, 0);
// }

// public bool point_equals(Point a, Point b) {
//   return a.x == b.x && a.y == b.y;
// }

// public bool same_sign_nonzero(Int128 a, Int128 b) {
//   if (i128_is_zero(a) || i128_is_zero(b))
//     return false;
//   return (i128_is_neg(a) && i128_is_neg(b)) || (!i128_is_neg(a) && !i128_is_neg(b));
// }

// public bool intersect_test(Edge e1, Point e1_p1, Point e1_p2, Edge e2, Point e2_p1, Point e2_p2)
// {
//   Int128 e2_p1_agst_e1 = subedge(e2_p1, e1);
//   Int128 e2_p2_agst_e1 = subedge(e2_p2, e1);
//   Int128 e1_p1_agst_e2 = subedge(e1_p1, e2);
//   Int128 e1_p2_agst_e2 = subedge(e1_p2, e2);

//   if (i128_is_zero(e1_p1_agst_e2))
//     e1_p1_agst_e2 = tie_break_neg(e2);
//   if (i128_is_zero(e1_p1_agst_e2))
//     return false;

//   if (i128_is_zero(e1_p2_agst_e2))
//     e1_p2_agst_e2 = tie_break_neg(e2);
//   if (i128_is_zero(e1_p2_agst_e2))
//     return false;

//   if (same_sign_nonzero(e1_p1_agst_e2, e1_p2_agst_e2))
//     return false;

//   if (i128_is_zero(e2_p1_agst_e1))
//     e2_p1_agst_e1 = tie_break_pos(e1);
//   if (i128_is_zero(e2_p1_agst_e1))
//     return false;

//   if (i128_is_zero(e2_p2_agst_e1))
//     e2_p2_agst_e1 = tie_break_pos(e1);
//   if (i128_is_zero(e2_p2_agst_e1))
//     return false;

//   if (same_sign_nonzero(e2_p1_agst_e1, e2_p2_agst_e1))
//     return false;

//   if ((point_equals(e1_p1, e2_p1) && point_equals(e1_p2, e2_p2)) ||
//       (point_equals(e1_p1, e2_p2) && point_equals(e1_p2, e2_p1))) {
//     return false;
//   }

//   return true;
// }

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
};

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

public Int128 i128_sign(Int128 x) {
  return i128_is_neg(x) ? i128_from_i64(-1) : i128_from_i64(1);
}

public int64_t i128_to_i64_lossless(Int128 x) {
  return (int64_t)x.lo;
}

/* ---------------------------
   64x64 -> 128
   --------------------------- */

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

/* ---------------------------
   Full signed 128x128 -> low 128
   This matches __int128 arithmetic as long as the true result fits in signed 128.
   --------------------------- */

public Int128 mul_i128_i128(Int128 a, Int128 b) {
  bool neg = (i128_is_neg(a) != i128_is_neg(b));

  Int128 aa = i128_abs(a);
  Int128 bb = i128_abs(b);

  Int128 p00 = mul_u64_u64(aa.lo, bb.lo);
  Int128 p01 = mul_u64_u64(aa.lo, (uint64_t)bb.hi);
  Int128 p10 = mul_u64_u64((uint64_t)aa.hi, bb.lo);
  // p11 term contributes only beyond bit 127, so it is discarded for 128-bit result.

  Int128 r;
  r.lo = p00.lo;
  r.hi = (int64_t)((uint64_t)p00.hi + p01.lo + p10.lo);

  return neg ? i128_neg(r) : r;
}

public Int128 subedge(Point p, Edge e) {
  return i128_add(
      i128_add(mul_i128_i128(i128_from_i64(p.x), e.a), mul_i128_i128(i128_from_i64(p.y), e.b)),
      e.c);
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

/* ---------------------------
   Unsigned / signed 128-bit helpers for gcd/div/mod
   --------------------------- */

public int i128_cmp_unsigned(Int128 a, Int128 b) {
  uint64_t ahi = (uint64_t)a.hi;
  uint64_t bhi = (uint64_t)b.hi;
  if (ahi < bhi)
    return -1;
  if (ahi > bhi)
    return 1;
  if (a.lo < b.lo)
    return -1;
  if (a.lo > b.lo)
    return 1;
  return 0;
}

public Int128 i128_sub_unsigned(Int128 a, Int128 b) {
  Int128 r;
  r.lo = a.lo - b.lo;
  uint64_t borrow = (a.lo < b.lo) ? 1ull : 0ull;
  r.hi = (int64_t)((uint64_t)a.hi - (uint64_t)b.hi - borrow);
  return r;
}

public Int128 i128_shl1(Int128 a) {
  uint64_t hiu = (uint64_t)a.hi;
  uint64_t new_hi = (hiu << 1) | (a.lo >> 63);
  uint64_t new_lo = a.lo << 1;
  return make_i128((int64_t)new_hi, new_lo);
}

public Int128 i128_shr1(Int128 a) {
  uint64_t hiu = (uint64_t)a.hi;
  uint64_t new_lo = (a.lo >> 1) | (hiu << 63);
  uint64_t new_hi = hiu >> 1;
  return make_i128((int64_t)new_hi, new_lo);
}

public Int128 i128_shl_bits(Int128 a, uint n) {
  Int128 r = a;
  for (uint i = 0; i < n; ++i) {
    r = i128_shl1(r);
  }
  return r;
}

public Int128 i128_shr_bits(Int128 a, uint n) {
  Int128 r = a;
  for (uint i = 0; i < n; ++i) {
    r = i128_shr1(r);
  }
  return r;
}

public bool i128_get_bit(Int128 a, uint bit) {
  if (bit < 64u) {
    return ((a.lo >> bit) & 1ull) != 0ull;
  }
  uint b = bit - 64u;
  return ((((uint64_t)a.hi) >> b) & 1ull) != 0ull;
}

public Int128 i128_set_bit(Int128 a, uint bit) {
  if (bit < 64u) {
    a.lo |= (1ull << bit);
  } else {
    uint b = bit - 64u;
    uint64_t hiu = (uint64_t)a.hi;
    hiu |= (1ull << b);
    a.hi = (int64_t)hiu;
  }
  return a;
}

public uint i128_ctz(Int128 a) {
  if (i128_is_zero(a))
    return 128u;

  if (a.lo != 0ull) {
    uint c = 0u;
    uint64_t v = a.lo;
    while ((v & 1ull) == 0ull) {
      v >>= 1;
      ++c;
    }
    return c;
  }

  uint64_t v = (uint64_t)a.hi;
  uint c = 64u;
  while ((v & 1ull) == 0ull) {
    v >>= 1;
    ++c;
  }
  return c;
}

public void i128_udivmod(Int128 num, Int128 den, out Int128 q, out Int128 r) {
  q = make_i128(0, 0);
  r = make_i128(0, 0);

  if (i128_is_zero(den)) {
    return;
  }

  for (int i = 127; i >= 0; --i) {
    r = i128_shl1(r);
    if (i128_get_bit(num, (uint)i)) {
      r.lo |= 1ull;
    }

    if (i128_cmp_unsigned(r, den) >= 0) {
      r = i128_sub_unsigned(r, den);
      q = i128_set_bit(q, (uint)i);
    }
  }
}

public Int128 i128_udiv_exact(Int128 num, Int128 den) {
  Int128 q, r;
  i128_udivmod(num, den, q, r);
  return q;
}

public Int128 i128_div_exact(Int128 num, Int128 den) {
  if (i128_is_zero(den)) {
    return make_i128(0, 0);
  }

  bool neg = (i128_is_neg(num) != i128_is_neg(den));
  Int128 un = i128_abs(num);
  Int128 ud = i128_abs(den);

  Int128 q = i128_udiv_exact(un, ud);
  return neg ? i128_neg(q) : q;
}

/* Stein's binary gcd: avoids modulo/div in gcd itself. */
public Int128 i128_gcd(Int128 a, Int128 b) {
  a = i128_abs(a);
  b = i128_abs(b);

  if (i128_is_zero(a))
    return i128_is_zero(b) ? i128_from_i64(1) : b;
  if (i128_is_zero(b))
    return a;

  uint shift = min(i128_ctz(a), i128_ctz(b));
  a = i128_shr_bits(a, i128_ctz(a));

  while (!i128_is_zero(b)) {
    b = i128_shr_bits(b, i128_ctz(b));

    if (i128_cmp_unsigned(a, b) > 0) {
      Int128 tmp = a;
      a = b;
      b = tmp;
    }

    b = i128_sub_unsigned(b, a);
  }

  return i128_shl_bits(a, shift);
}

/* ---------------------------
   Rational helpers matching tcb::rational simplify()
   --------------------------- */

public Rational128 rational128_make(Int128 num, Int128 den) {
  Rational128 r;

  if (i128_is_zero(den)) {
    r.num = num;
    r.den = den;
    return r;
  }

  Int128 g = i128_gcd(num, den);
  g = i128_abs(g);

  num = i128_div_exact(num, g);
  den = i128_div_exact(den, g);

  if (i128_is_neg(den)) {
    num = i128_neg(num);
    den = i128_neg(den);
  }

  r.num = num;
  r.den = den;
  return r;
}

public bool rational128_lt_i64(Rational128 r, int64_t v) {
  return i128_lt(r.num, mul_i128_i128(r.den, i128_from_i64(v)));
}

public bool rational128_gt_i64(Rational128 r, int64_t v) {
  return i128_gt(r.num, mul_i128_i128(r.den, i128_from_i64(v)));
}
