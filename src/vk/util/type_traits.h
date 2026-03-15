#ifndef RAYJOIN_TYPE_TRAITS_H
#define RAYJOIN_TYPE_TRAITS_H

template<typename T>
struct alignas(16) Vec2 {
  T x, y;
};

template<typename T>
struct Vec3 {
  T x, y, z;
};

template<typename T>
concept PointCoordType = std::same_as<T, float> || std::same_as<T, double>;

template<typename T>
concept EdgeCoeffType = std::same_as<T, float> || std::same_as<T, double>;

#endif  // RAYJOIN_TYPE_TRAITS_H
