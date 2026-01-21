#ifndef RAYJOIN_UTIL_H
#define RAYJOIN_UTIL_H
#include <cuda_runtime.h>
#include <thrust/host_vector.h>
#include <thrust/mr/allocator.h>
#include <thrust/system/cuda/memory_resource.h>

#if defined(__CUDACC__) || defined(__CUDABE__)
#define DEV_HOST __device__ __host__
#define DEV_HOST_INLINE __device__ __host__ __forceinline__
#define DEV_INLINE __device__ __forceinline__
#define CONST_STATIC_INIT(...)
#else
#define DEV_HOST
#define DEV_HOST_INLINE
#define DEV_INLINE
#define CONST_STATIC_INIT(...) = __VA_ARGS__
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX4(a, b, c, d) (MAX(MAX(a, b), MAX(c, d)))
#define MIN4(a, b, c, d) (MIN(MIN(a, b), MIN(c, d)))

#define SWAP(i, j) \
  {                \
    auto temp = i; \
    i = j;         \
    j = temp;      \
  }

#define MAX_BLOCK_SIZE (256)
#define MAX_GRID_SIZE (768)
#define MAX_BLOCK_SIZE_2D (16)
#define TID_1D (threadIdx.x + blockIdx.x * blockDim.x)
#define TOTAL_THREADS_1D (gridDim.x * blockDim.x)
#define THRUST_TO_CUPTR(x) \
  (reinterpret_cast<CUdeviceptr>(thrust::raw_pointer_cast(x)))

#define OPTIX_TID_1D                                        \
  (optixGetLaunchIndex().x +                                \
   optixGetLaunchIndex().y * optixGetLaunchDimensions().x + \
   optixGetLaunchIndex().z * optixGetLaunchDimensions().x * \
       optixGetLaunchDimensions().y)
#define OPTIX_TOTAL_THREADS_1D                                   \
  (optixGetLaunchDimensions().x * optixGetLaunchDimensions().y * \
   optixGetLaunchDimensions().z)
static inline __host__ __device__ size_t round_up(size_t numerator,
                                                  size_t denominator) {
  return (numerator + denominator - 1) / denominator;
}
inline void KernelSizing(int& block_num, int& block_size, size_t work_size) {
  block_size = MAX_BLOCK_SIZE;
  block_num = std::min(MAX_GRID_SIZE, (int) round_up(work_size, block_size));
}

inline void KernelSizing(dim3& grid_dims, dim3& block_dims, size_t work_size) {
  block_dims = {MAX_BLOCK_SIZE, 1, 1};
  grid_dims = {(unsigned int) round_up(work_size, block_dims.x), 1, 1};
}
// CUDA 12
using mr_pinned = thrust::system::cuda::universal_host_pinned_memory_resource;
template <typename T>
using pinned_vector =
    thrust::host_vector<T,
                        thrust::mr::stateless_resource_allocator<T, mr_pinned>>;

#define SQ(x) ((x) * (x))
#define SIZEOF_ELEM(vec) \
  (sizeof(typename std::remove_reference<decltype(vec)>::type::value_type))
#define COUNT_CONTAINER_BYTES(vec) (vec.size() * SIZEOF_ELEM(vec))

namespace std {

template <>
struct hash<double2> {
  size_t operator()(const double2& p) const noexcept {
    return *reinterpret_cast<const int64_t*>(&p.x) ^
           *reinterpret_cast<const int64_t*>(&p.y);
  }
};

// move == outside of namespace std will fix the lack of == for double2 problem
// inline bool operator==(const double2& p1, const double2& p2) {
//   return p1.x == p2.x && p1.y == p2.y;
// }

}  // namespace std

inline bool operator==(const double2& p1, const double2& p2) {
  return p1.x == p2.x && p1.y == p2.y;
}


#endif  // RAYJOIN_UTIL_H
