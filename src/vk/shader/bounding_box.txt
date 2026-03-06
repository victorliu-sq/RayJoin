namespace rayjoin {
  struct BoundingBox<T> {
    T min_x;
    T min_y;
    T max_x;
    T max_y;
  };

  BoundingBox<double> bboxInit() {
    BoundingBox<double> b;

    double dmax = 1.7976931348623157e308;  // around DBL_MAX

    b.min_x = dmax;
    b.min_y = dmax;
    b.max_x = -dmax;
    b.max_y = -dmax;

    return b;
  }
}
