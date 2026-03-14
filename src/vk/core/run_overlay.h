#ifndef RAYJOIN_RUN_OVERLAY_CUH
#define RAYJOIN_RUN_OVERLAY_CUH
#include "core/overlay_config.h"

namespace rayjoin {
namespace vk {
void RunOverlay(const OverlayConfig& config);

// no scaling

void RunOverlayNS(const OverlayConfig& config);
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_RUN_OVERLAY_CUH
