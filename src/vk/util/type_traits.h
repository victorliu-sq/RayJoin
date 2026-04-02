#ifndef RAYJOIN_TYPE_TRAITS_H
#define RAYJOIN_TYPE_TRAITS_H

#include "shader/config.h"

namespace rayjoin {
namespace vk {

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


inline std::string Int128ToString(__int128 v) {
  if (v == 0) return "0";

  bool neg = v < 0;
  unsigned __int128 x = neg ? static_cast<unsigned __int128>(-v) : static_cast<unsigned __int128>(v);

  std::string s;
  while (x > 0) {
    s.push_back(static_cast<char>('0' + (x % 10)));
    x /= 10;
  }
  if (neg) s.push_back('-');
  std::reverse(s.begin(), s.end());
  return s;
}

struct alignas(16) Rational128 {
  __int128 num;
  __int128 den;
};

struct alignas(16) Intersection128 {
  Rational128 x;
  Rational128 y;
  index_t eid0;
  index_t eid1;
  polygon_id_t mid_point_polygon_id;
  uint32_t pad;
};

static_assert(sizeof(Rational128) == 32);
static_assert(alignof(Rational128) == 16);
static_assert(sizeof(Intersection128) == 80);
static_assert(alignof(Intersection128) == 16);
static_assert(std::is_trivially_copyable_v<Intersection128>);

}  // namespace vk
}  // namespace rayjoin


#endif  // RAYJOIN_TYPE_TRAITS_H
