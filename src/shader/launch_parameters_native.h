#ifndef RAYJOIN_LAUNCH_PARAMETERS_NATIVE_H
#define RAYJOIN_LAUNCH_PARAMETERS_NATIVE_H

#include <cstdint>
#include <thrust/tuple.h>

#include "config.h"
#include "lsi_native.h"
#include "map/map_native.h"
#include "map/scaling.h"
#include "shader/lsi.h"
#include "util/array_view.h"
#include "util/bitset.h"
#include "util/queue.h"

namespace rayjoin {
struct LaunchParamsLSINative {
  using coord_t = coord_t;
  using map_t = dev::MapNativeDev<coord_t>;
  using edge_t = typename map_t::edge_t;
  using point_t = typename map_t::point_t;
  using xsect_t = dev::IntersectionNative<coord_t>;

  edge_t* base_edges;
  point_t* base_points;
  thrust::pair<size_t, size_t>* eid_range;

  int query_map_id;
  ArrayView<edge_t> query_edges;
  point_t* query_points;

  OptixTraversableHandle traversable;

  dev::Queue<xsect_t, uint32_t> xsects;

#ifndef NDEBUG
  uint32_t* n_tests;
#endif
};

}  // namespace rayjoin

#endif  // RAYJOIN_LAUNCH_PARAMETERS_NATIVE_H
