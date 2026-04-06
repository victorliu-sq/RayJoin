#ifndef RAYJOIN_PIP_NATIVE_H
#define RAYJOIN_PIP_NATIVE_H

namespace rayjoin {

template<typename CONTEXT_T>
class PIPNative {
  using coord_t = typename CONTEXT_T::coord_t;
  using map_t = typename CONTEXT_T::map_t;
  using point_t = typename map_t::point_t;

 public:
  explicit PIPNative(CONTEXT_T& ctx) : ctx_(ctx) {}
  virtual ~PIPNative() = default;

  virtual void Init(size_t n_points) { closest_eids_.reserve(n_points); }

  virtual void Query(Stream& stream, int query_map_id, ArrayView<point_t> query_points) = 0;

  CONTEXT_T& get_context() { return ctx_; }
  const CONTEXT_T& get_context() const { return ctx_; }

  const thrust::device_vector<index_t>& get_closest_eids() const { return closest_eids_; }

 protected:
  CONTEXT_T& ctx_;
  thrust::device_vector<index_t> closest_eids_;
  SharedValue<uint64_t> prof_counter_;
};

}  // namespace rayjoin

#endif  // RAYJOIN_PIP_NATIVE_H
