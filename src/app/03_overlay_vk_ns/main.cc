#include "util/guard_gflag.h"
#include "util/guard_glog.h"
#include "vk/core/run_overlay_ns.h"
#include "vk/core/vk_global_context.h"

int main(int argc, char* argv[]) {
  auto gflag_guard = CreateGflagsGuard(argc, argv, "Usage: -poly1 -poly2");
  auto glog_guard = CreateGlogGuardAlsoToStderr("polyover_vk");
  auto vk_runtime = rayjoin::vk::CreateVkGlobalRuntime();

  auto config = rayjoin::CreateOverlayConfig(argv[0]);
  rayjoin::vk::RunOverlayNS(config);
}
