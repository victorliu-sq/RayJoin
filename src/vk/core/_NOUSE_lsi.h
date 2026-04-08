#ifndef RAYJOIN_LSI_H
#define RAYJOIN_LSI_H
#include "glog/logging.h"
#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_helpers.h"
#include "vk_global_context.h"

namespace rayjoin {
namespace vk {

struct Rational64 {
  int64_t num;
  int64_t den;
};

struct IntersectionR {
  Rational64 x;
  Rational64 y;

  uint64_t eid0;
  uint64_t eid1;

  uint32_t mid_point_polygon_id;
  uint32_t pad;
};

static_assert(sizeof(Rational64) == 16, "Rational64 must be 16 bytes");
static_assert(alignof(Rational64) == 8, "Rational64 alignment must be 8");
static_assert(sizeof(IntersectionR) == 56, "Intersection must be 56 bytes");
static_assert(alignof(IntersectionR) == 8, "Intersection alignment must be 8");

template<typename CONTEXT_T>
class LSI {
 protected:
  using coord_t = typename CONTEXT_T::coord_t;
  using internal_coord_t = typename CONTEXT_T::internal_coord_t;
  using coefficient_t = typename CONTEXT_T::coefficient_t;
  // double, int64_t, and int64_t

 public:
  // using xsect_t = dev::Intersection<internal_coord_t>;
  // using xsect_t = Intersection;

  explicit LSI(CONTEXT_T &ctx) : ctx_(ctx) {}

  // virtual ~LSI() = default;
  virtual ~LSI() {
    auto &vk_ctx = GetVkComputeContext();

    // vmaDestroyBufferSafe(vk_ctx.vma, xsect_dev_);
    // vmaDestroyBufferSafe(vk_ctx.vma, prof_counter_);
  }

  virtual void Init(size_t max_n_xsects) {
    // LOG(INFO) << "Queue size: " << max_n_xsects * sizeof(xsect_t) / 1024 /
    // 1024
    //           << " MB";
    // xsect_queue_.Init(max_n_xsects);
    auto &vk_ctx = GetVkComputeContext();
    // xsect_dev_ = createStorageBuffer<xsect_t>(vk_ctx.vma, max_n_xsects);
    // prof_counter_ = createStorageBuffer<uint64_t>(vk_ctx.vma, 1);
    // xsect_dev_ =
    //     createStorageBuffer(vk_ctx.vma, sizeof(xsect_t) * max_n_xsects);
    // prof_counter_ = createStorageBuffer(vk_ctx.vma, sizeof(uint64_t) * 1);

    xsect_capacity_ = max_n_xsects;
    xsect_dev_.Init(sizeof(IntersectionR) * max_n_xsects);
    xsect_counter_.Init(sizeof(uint32_t));  // NEW
    prof_counter_.Init(sizeof(uint32_t) * 20);  // debug values
  }

  virtual void Query(int query_map_id) = 0;


  CONTEXT_T &get_context() { return ctx_; }
  const CONTEXT_T &get_context() const { return ctx_; }

  VkDeviceBuf &get_xsect_buffer() { return xsect_dev_; }
  const VkDeviceBuf &get_xsect_buffer() const { return xsect_dev_; }

  VkDeviceBuf &get_prof_counter_buffer() { return prof_counter_; }
  const VkDeviceBuf &get_prof_counter_buffer() const { return prof_counter_; }

  size_t get_xsect_capacity() const { return xsect_capacity_; }

  VkDeviceBuf &get_xsect_counter_buffer() { return xsect_counter_; }
  const VkDeviceBuf &get_xsect_counter_buffer() const { return xsect_counter_; }

 protected:
  CONTEXT_T &ctx_;

  // Queue<xsect_t> xsect_queue_;
  // SharedValue<uint64_t> prof_counter_;

  VkDeviceBuf xsect_dev_{};
  VkDeviceBuf xsect_counter_{};

  VkDeviceBuf prof_counter_{};

  size_t xsect_capacity_ = 0;
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_LSI_H
