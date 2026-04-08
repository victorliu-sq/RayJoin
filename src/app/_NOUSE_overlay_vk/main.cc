#include <glog/logging.h>

#include "../../vk/core/vk_global_context.h"
#include "flag/flags.h"
#include "util/guard_gflag.h"
#include "util/guard_glog.h"
#include "vk/core/run_overlay.h"

int main(int argc, char* argv[]) {
  FLAGS_stderrthreshold = 0;
  auto flags = CreateGflagsGuard(argc, argv, "Usage: -poly1 -poly2");
  auto g_glog_guard = CreateGlogGuard("polyover_vk");
  auto vk_runtime = CreateVkGlobalRuntime();

  rayjoin::OverlayConfig config;
  std::string exec_path = argv[0];

  config.map1_path = FLAGS_poly1;
  config.map2_path = FLAGS_poly2;
  config.output_path = FLAGS_output;
  config.serialize_prefix = FLAGS_serialize;
  config.grid_size = FLAGS_grid_size;
  config.xsect_factor = FLAGS_xsect_factor;
  config.mode = FLAGS_mode;
  config.exec_root = exec_path.substr(0, exec_path.find_last_of("/"));
  config.check = FLAGS_check;
  config.fau = FLAGS_fau;
  config.profile = FLAGS_profile;
  config.win = FLAGS_win;
  config.ag = FLAGS_ag;
  config.ag_iter = FLAGS_ag_iter;
  config.enlarge = FLAGS_enlarge;

  // dump
  config.dump_results = FLAGS_dump_results;
  config.dump_dir = FLAGS_dump_dir;

  rayjoin::vk::RunOverlay(config);
}
