#ifndef RAYJOIN_MAP_OVERLAY_RT_H
#define RAYJOIN_MAP_OVERLAY_RT_H

#include "vk/core/lsi_rt.h"
#include "vk/rt/rt_engine.h"

namespace rayjoin {
namespace vk {

template <typename CONTEXT_T>
class MapOverlayRT : public MapOverlay<CONTEXT_T> {
 public:
  explicit MapOverlayRT(CONTEXT_T& ctx) : MapOverlay<CONTEXT_T>(ctx) {
    rt_engine_ = std::make_shared<RTEngine>();
    this->lsi_ = std::make_shared<LSIRT<CONTEXT_T>>(ctx, rt_engine_);
    // this->pip_ = std::make_shared<PIPRT<CONTEXT_T>>(ctx, rt_engine_);
  }

  void set_config(const QueryConfigRT& config) { config_ = config; }

  void Init() override {
    auto& ctx = this->ctx_;
    auto& lsi = this->lsi_;
    // Initialize RT Engine

    // Initialize LSI
    lsi->Init(ctx.get_edge_num() * config_.xsect_factor);
  }

  void BuildIndex() override {}

  void IntersectEdge(int query_map_id) override {}

  void LocateVerticesInOtherMap(int query_map_id) override {}

  void ComputeOutputPolygons() override {}

  void WriteResult(const char* path) override {}

 private:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;

  // Queue<xsect_t> xsect_queue_;
};

}  // namespace vk
}  // namespace rayjoin
#endif  // RAYJOIN_MAP_OVERLAY_RT_H
