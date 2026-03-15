#ifndef RAYJOIN_MAP_OVERLAY_RT_NS_H
#define RAYJOIN_MAP_OVERLAY_RT_NS_H

#include "map_overlay_ns.h"
#include "query_config.h"
#include "vk/core/lsi_rt.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/map.h"
#include "vk/map/vk_debug_readback.h"
#include "vk/rt/primitives.h"
#include "vk/rt/rt_engine.h"

namespace rayjoin {
namespace vk {
template<typename CONTEXT_T>
class MapOverlayRTNS : public MapOverlayNS<CONTEXT_T> {
  // using map_t = typename CONTEXT_T::map_t;

 public:
  explicit MapOverlayRTNS(CONTEXT_T &ctx) : MapOverlayNS<CONTEXT_T>(ctx) {}

  void set_config(const QueryConfigRT &config) { config_ = config; }

  void Init() override {}

  void BuildIndex() override {}

  void IntersectEdge(int query_map_id) override {}

  void LocateVerticesInOtherMap(int query_map_id) override {}

  void ComputeOutputPolygons() override {}

  void WriteResult(const char *path) override {}

 private:
  QueryConfigRT config_;
};


}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_MAP_OVERLAY_RT_NS_H
