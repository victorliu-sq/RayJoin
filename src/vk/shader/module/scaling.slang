module scaling;

public struct Scaling {
  int64_t internal_max;
  int64_t internal_min;
  int64_t internal_range;

  double rx;
  double ry;
  double rrx;
  double rry;

  double deltax;
  double deltay;

  double ddeltax;
  double ddeltay;

  public inline int64_t ScaleX(double x) {
    return (int64_t) (x * rx + deltax);
  }

  public inline int64_t ScaleY(double y) {
    return (int64_t) (y * ry + deltay);
  }

  public inline double UnscaleX(int64_t internal_x) {
    return internal_x * rrx + ddeltax;
  }

  public inline double UnscaleY(int64_t internal_y) {
    return internal_y * rry + ddeltay;
  }
};
