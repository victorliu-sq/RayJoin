#ifndef RAYJOIN_PIP_RT_NATVE_H
#define RAYJOIN_PIP_RT_NATVE_H

#include <thrust/host_vector.h>
#include <utility>

#include "core/query_config.h"
#include "rt/rt_engine.h"

namespace rayjoin {

template<typename CONTEXT_T>
class PIPRTNative : public PIPNative<CONTEXT_T> {
  using coord_t = typename CONTEXT_T::coord_t;
  using map_t = typename CONTEXT_T::map_t;
  using point_t = typename map_t::point_t;

 public:
  explicit PIPRTNative(CONTEXT_T& ctx, std::shared_ptr<RTEngine> rt_engine) : PIPNative<CONTEXT_T>(ctx), rt_engine_(std::move(rt_engine)) {}

  virtual ~PIPRTNative() = default;

  void set_config(const QueryConfigRT& query_config) { config_ = query_config; }

  void Query(Stream& stream, int query_map_id, ArrayView<point_t> d_query_points) override {
    auto& ctx = this->ctx_;
    const auto base_map_id = 1 - query_map_id;
    auto d_base_map = ctx.get_map(base_map_id)->DeviceObject();
    const auto points_num = d_query_points.size();

    this->closest_eids_.resize(points_num);
    best_ys_.resize(points_num);

    // Patch: fix the test basic 1
    if (points_num == 0) {
      return;
    }

    thrust::fill(this->closest_eids_.begin(), this->closest_eids_.end(), DONTKNOW);
    thrust::fill(best_ys_.begin(), best_ys_.end(), std::numeric_limits<double>::infinity());

    LaunchParamsPIPNative params;
    params.base_map_edges = d_base_map.get_edges().data();
    params.base_map_points = d_base_map.get_points().data();
    params.eid_range = thrust::raw_pointer_cast(config_.eid_range->data());
    params.query_map_id = query_map_id;
    params.query_points = d_query_points;
    params.traversable = config_.handle;
    params.closest_eids = thrust::raw_pointer_cast(this->closest_eids_.data());
    params.best_ys = thrust::raw_pointer_cast(best_ys_.data());

#ifndef NDEBUG
    hit_count_.resize(points_num, 0);
    closer_count_.resize(points_num, 0);
    last_update_count_.resize(points_num, 0);
    fail_update_count_.resize(points_num, 0);

    params.hit_count = ArrayView<uint32_t>(hit_count_).data();
    params.closer_count = ArrayView<uint32_t>(closer_count_).data();
    params.last_update_count = ArrayView<uint32_t>(last_update_count_).data();
    params.fail_update_count = ArrayView<uint32_t>(fail_update_count_).data();
#endif

    rt_engine_->CopyLaunchParams(stream, params);
    rt_engine_->Render(stream, ModuleIdentifier::MODULE_ID_PIP_CUSTOM_NATIVE, dim3{static_cast<unsigned int>(points_num), 1, 1});
    stream.Sync();

#ifndef NDEBUG
    if (config_.profile) {
      thrust::host_vector<uint32_t> hit_count = hit_count_;
      uint32_t n_tests = 0;
      for (auto& n: hit_count) n_tests += n;
      LOG(INFO) << "Total tests: " << n_tests;
    }
#endif
  }

  thrust::device_vector<double>& get_best_ys() { return best_ys_; }
  const thrust::device_vector<double>& get_best_ys() const { return best_ys_; }

 protected:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;
  thrust::device_vector<double> best_ys_;

#ifndef NDEBUG
  thrust::device_vector<uint32_t> hit_count_;
  thrust::device_vector<uint32_t> closer_count_;
  thrust::device_vector<uint32_t> last_update_count_;
  thrust::device_vector<uint32_t> fail_update_count_;
#endif
};
}  // namespace rayjoin


#endif  // RAYJOIN_PIP_RT_NATVE_H
