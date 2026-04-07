#include <glog/logging.h>

#include "flag/flags.h"
#include "util/guard_gflag.h"
#include "util/guard_glog.h"
#include "vk/core/run_overlay.h"
#include "vk/core/vk_global_context.h"

int main(int argc, char* argv[]) {
  auto gflag_guard = CreateGflagsGuardWithStderrInfo(argc, argv, "Usage: -poly1 -poly2");
  auto glog_guard = CreateGlogGuard("polyover_vk");

  auto config = rayjoin::CreateOverlayConfig(argv[0]);
  rayjoin::vk::RunOverlayNS(config);
}
