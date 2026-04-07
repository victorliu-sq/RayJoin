#include <glog/logging.h>

#include "core/run_overlay.h"
#include "flag/flags.h"
#include "util/guard_gflag.h"
#include "util/guard_glog.h"

int main(int argc, char* argv[]) {
  auto gflag_guard = CreateGflagsGuardWithStderrInfo(argc, argv, "Usage: -poly1 -poly2");
  auto glog_guard = CreateGlogGuard("polyover_vk");

  auto config = rayjoin::CreateOverlayConfig(argv[0]);
  rayjoin::RunOverlay(config);
}
