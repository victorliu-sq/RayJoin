#include <array>
#include <memory>

#include "core/run_overlay.h"
#include "glog/logging.h"
#include "map/context_native.h"
#include "map/planar_graph.h"
// #include "util/rational.h"
#include "map_overlay_native.h"
#include "map_overlay_native_rt.h"
#include "util/stopwatch.h"
#include "util/timer.h"

namespace rayjoin {
void RunOverlayNative(const OverlayConfig& config) {
  using context_t = ContextNative<coord_t>;
  timer_start();

  timer_next("Read map 0");
  LOG(INFO) << "Reading map 0 from " << config.map1_path;
  auto g1 = load_from<coord_t>(config.map1_path, config.serialize_prefix);

  timer_next("Read map 1");
  LOG(INFO) << "Reading map 1 from " << config.map2_path;
  auto g2 = load_from<coord_t>(config.map2_path, config.serialize_prefix);

  timer_next("Create App");
  context_t ctx({g1, g2});
  std::shared_ptr<MapOverlayNative<context_t>> overlay;

  QueryConfigRT query_config;
  query_config.dump_results = config.dump_results;
  query_config.dump_dir = config.dump_dir;

  timer_next("Load Data");
  // ctx.LoadToDevice();
  ctx.LoadToDevice(query_config);

  if (config.mode == "rt") {
    auto overlay_rt = std::make_shared<MapOverlayNativeRT<context_t>>(ctx);

    query_config.profile = config.profile;
    query_config.fau = config.fau;
    query_config.xsect_factor = config.xsect_factor;
    query_config.ag = config.ag;
    query_config.ag_iter = config.ag_iter;
    query_config.win = config.win;
    query_config.enlarge = config.enlarge;

    // new dump config
    overlay_rt->set_config(query_config);
    overlay = overlay_rt;
  }
  // else if (config.mode == "grid") {
  //   auto overlay_grid = std::make_shared<MapOverlayGrid<context_t>>(ctx);
  //   QueryConfigGrid query_config;
  //
  //   query_config.grid_size = config.grid_size;
  //   query_config.profile = config.profile;
  //   query_config.xsect_factor = config.xsect_factor;
  //  overlay_grid->set_config(query_config);
  //   overlay = overlay_grid;
  // } else if (config.mode == "lbvh") {
  //   auto overlay_lbvh = std::make_shared<MapOverlayLBVH<context_t>>(ctx);
  //   QueryConfigLBVH query_config;
  //
  //   query_config.profile = config.profile;
  //   query_config.xsect_factor = config.xsect_factor;
  //
  //   overlay_lbvh->set_config(query_config);
  //   overlay = overlay_lbvh;
  // }
  else {
    LOG(FATAL) << "Illegal mode: " << config.mode;
  }

  timer_next("Init");
  overlay->Init();

  timer_next("Build Index");
  overlay->BuildIndex();

  timer_next("Intersection edges");
  overlay->IntersectEdge(0);

  FOR2 {
    auto prefix = "Map " + std::to_string(im) + ": ";

    timer_next(prefix + "Locate vertices in other map");
    // overlay->LocateVerticesInOtherMap(im);
  }

  timer_next("Computer output polygons");
  // overlay->ComputeOutputPolygons();

  if (config.check) {
    timer_next("Check result");
    // CheckResult(ctx, overlay, config);
  }

  if (!config.output_path.empty()) {
    timer_next("Write to file");
    // overlay->WriteResult(config.output_path.c_str());
  }
  timer_end();
}

}  // namespace rayjoin
