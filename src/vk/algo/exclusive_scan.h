#ifndef RAYJOIN_EXCLUSIVE_SCAN_H
#define RAYJOIN_EXCLUSIVE_SCAN_H

#include <cstdint>

#include "vk/engine/vk_buffer.h"
#include "vk/engine/vk_buffer_readback.h"
#include "vk/engine/vk_compute_engine.h"
#include "vk/util/type_native.h"

namespace rayjoin::vk {
namespace algo {

inline uint32_t DivUpU32(uint32_t a, uint32_t b) { return (a + b - 1u) / b; }

inline void ExclusiveScanUInt32(const VkDeviceBuf& in_buf, const VkDeviceBuf& out_buf, uint32_t count) {
  if (count == 0u) {
    return;
  }

  constexpr uint32_t kBlockSize = 256u;
  const uint32_t num_blocks = DivUpU32(count, kBlockSize);

  struct LaunchParamsScan {
    uint32_t count;
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
  };

  const std::string scan_block_spv = std::string(SHADER_KERNEL_NS_DIR) + "/scan_uint32_block_ns.spv";
  const std::string add_offsets_spv = std::string(SHADER_KERNEL_NS_DIR) + "/scan_uint32_add_offsets_ns.spv";

  // Base case: one block
  if (num_blocks == 1u) {
    VkDeviceBuf dummy_block_sums_buf;
    dummy_block_sums_buf.Init(sizeof(uint32_t));

    RunComputePass(kBlockSize,
                   scan_block_spv.c_str(),
                   LaunchParamsScan{.count = count, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                   in_buf,  // binding 0 -> gIn
                   out_buf,  // binding 1 -> gOut
                   dummy_block_sums_buf);  // binding 2 -> gBlockSums
    return;
  }

  // Recursive case
  VkDeviceBuf block_sums_buf;
  block_sums_buf.Init(sizeof(uint32_t) * num_blocks);

  RunComputePass(num_blocks * kBlockSize,
                 scan_block_spv.c_str(),
                 LaunchParamsScan{.count = count, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                 in_buf,  // binding 0 -> gIn
                 out_buf,  // binding 1 -> gOut
                 block_sums_buf);  // binding 2 -> gBlockSums

  VkDeviceBuf block_offsets_buf;
  block_offsets_buf.Init(sizeof(uint32_t) * num_blocks);

  // Recursively exclusive-scan block sums
  ExclusiveScanUInt32(block_sums_buf, block_offsets_buf, num_blocks);

  // Add block offsets back to each element
  RunComputePass(num_blocks * kBlockSize,
                 add_offsets_spv.c_str(),
                 LaunchParamsScan{.count = count, ._pad0 = 0u, ._pad1 = 0u, ._pad2 = 0u},
                 out_buf,  // binding 0 -> gData
                 block_offsets_buf);  // binding 1 -> gBlockOffsets
}

}  // namespace algo
}  // namespace rayjoin::vk

#endif  // RAYJOIN_EXCLUSIVE_SCAN_H
