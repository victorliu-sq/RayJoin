#ifndef RAYJOIN_LSI_RT_NATIVE_H
#define RAYJOIN_LSI_RT_NATIVE_H

#include <thrust/sort.h>
#include <thrust/unique.h>
#include <utility>

#include "core/lsi.h"
#include "core/query_config.h"
#include "rt/rt_engine.h"
#include "shader/launch_parameters_native.h"
#include "shader/lsi.h"
#include "util/helpers.h"

namespace rayjoin {

template<typename CONTEXT_T>
class LSIRTNative : public LSINative<CONTEXT_T> {
  using base_t = LSINative<CONTEXT_T>;
  using coord_t = typename CONTEXT_T::coord_t;
  using xsect_t = typename base_t::xsect_t;

 public:
  explicit LSIRTNative(CONTEXT_T& ctx, const std::shared_ptr<RTEngine>& rt_engine) : LSINative<CONTEXT_T>(ctx), rt_engine_(rt_engine) {}

  void Init(size_t max_n_xsects) override { base_t::Init(max_n_xsects); }

  void Query(Stream& stream, int query_map_id) override {
    auto& ctx = this->ctx_;
    const int base_map_id = 1 - query_map_id;

    auto d_query_map = ctx.get_map(query_map_id)->DeviceObject();
    auto d_base_map = ctx.get_map(base_map_id)->DeviceObject();
    auto& xsects_queue = this->xsect_queue_;

    LaunchParamsLSINative params;
    params.query_map_id = query_map_id;
    params.base_edges = d_base_map.get_edges().data();
    params.base_points = d_base_map.get_points().data();
    params.eid_range = thrust::raw_pointer_cast(config_.eid_range->data());
    params.query_edges = d_query_map.get_edges();
    params.query_points = d_query_map.get_points().data();
    params.traversable = config_.handle;
    params.xsects = xsects_queue.DeviceObject();

#ifndef NDEBUG
    params.n_tests = n_tests_.data();
    n_tests_.set(0, stream);
#endif

    xsects_queue.Clear(stream);

    rt_engine_->CopyLaunchParams(stream, params);
    rt_engine_->Render(
        stream, ModuleIdentifier::MODULE_ID_LSI_CUSTOM_NATIVE, dim3{static_cast<unsigned int>(ctx.get_map(query_map_id)->get_edges_num()), 1, 1});

    const size_t n_xsects = xsects_queue.size(stream);

    ForEach(
        stream,
        n_xsects,
        [=] __device__(size_t idx, ArrayView<xsect_t> xsects) mutable {
          auto& xsect = xsects[idx];

          const auto base_eid = xsect.eid[base_map_id];
          const auto query_eid = xsect.eid[query_map_id];

          const auto& base_e = d_base_map.get_edge(base_eid);
          const auto& query_e = d_query_map.get_edge(query_eid);

          const coord_t denom = base_e.a * query_e.b - query_e.a * base_e.b;
          const coord_t numx = query_e.c * base_e.b - base_e.c * query_e.b;
          const coord_t numy = query_e.a * base_e.c - base_e.a * query_e.c;

          if (fabs((double) denom) <= 1e-15) {
            xsect.x = 0;
            xsect.y = 0;
            return;
          }

          coord_t x = numx / denom;
          coord_t y = numy / denom;

          const auto& base_e_p1 = d_base_map.get_point(base_e.p1_idx);
          const auto& base_e_p2 = d_base_map.get_point(base_e.p2_idx);
          const auto& query_e_p1 = d_query_map.get_point(query_e.p1_idx);
          const auto& query_e_p2 = d_query_map.get_point(query_e.p2_idx);

          auto t = MIN4(base_e_p1.x, base_e_p2.x, query_e_p1.x, query_e_p2.x);
          if (x < t) x = t;
          t = MAX4(base_e_p1.x, base_e_p2.x, query_e_p1.x, query_e_p2.x);
          if (x > t) x = t;

          t = MIN4(base_e_p1.y, base_e_p2.y, query_e_p1.y, query_e_p2.y);
          if (y < t) y = t;
          t = MAX4(base_e_p1.y, base_e_p2.y, query_e_p1.y, query_e_p2.y);
          if (y > t) y = t;

          xsect.x = x;
          xsect.y = y;
        },
        ArrayView<xsect_t>(xsects_queue.data(), n_xsects));

    stream.Sync();

    LOG(INFO) << "# of xsects: " << xsects_queue.size();

#ifndef NDEBUG
    if (config_.profile) {
      LOG(INFO) << "Total tests: " << n_tests_.get(stream);
    }
#endif
  }

  const QueryConfigRT& get_config() const { return config_; }
  void set_config(QueryConfigRT config) { config_ = std::move(config); }

 private:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;
  SharedValue<uint32_t> n_tests_;
};

}  // namespace rayjoin

#endif  // RAYJOIN_LSI_RT_NATIVE_H
