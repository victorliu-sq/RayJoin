#ifndef RAYJOIN_LSI_RT_H
#define RAYJOIN_LSI_RT_H

namespace rayjoin {
namespace vk {

template <typename CONTEXT_T>
class LSIRT : public LSI<CONTEXT_T> {
 public:
  // super class
  using lsi = LSI<CONTEXT_T>;

  explicit LSIRT(CONTEXT_T& ctx, const std::shared_ptr<RTEngine>& rt_engine)
      : LSI<CONTEXT_T>(ctx), rt_engine_(rt_engine) {}

  // Same as before
  void Init(size_t max_n_xsects) override { lsi::Init(max_n_xsects); }

  // use rt-engine
  void Query(int query_map_id) override {}

 private:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;
  // SharedValue<uint32_t> n_tests_;
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_LSI_RT_H
