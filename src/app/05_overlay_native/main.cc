#include <glog/logging.h>

// #include "core/run_overlay.h"
#include "core/run_overlay_native.h"
#include "flag/flags.h"
#include "util/guard_gflag.h"
#include "util/guard_glog.h"

int main(int argc, char* argv[]) {
  auto flags = CreateGflagsGuard(argc, argv, "Usage: -poly1 -poly2");
  auto g_glog_guard = CreateGlogGuardWithInfoOnStderr("polyover_vk");

  auto config = rayjoin::CreateOverlayConfig(argv[0]);
  rayjoin::RunOverlayNative(config);
}
