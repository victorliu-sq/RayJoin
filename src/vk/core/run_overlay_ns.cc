#include <array>
#include <memory>

#include "core/overlay_config.h"
#include "map_overlay_rt_ns.h"
#include "query_config.h"
#include "shader/config.h"
#include "util/stopwatch.h"
#include "util/timer.h"
#include "vk/core/map_overlay.h"
#include "vk/core/map_overlay_rt.h"
#include "vk/core/run_overlay.h"
#include "vk/map/context.h"
#include "vk/map/context_ns.h"
#include "vk/map/planar_graph.h"

namespace rayjoin {
namespace vk {

void RunOverlayNS(const rayjoin::OverlayConfig& config) {
  using context_t = ContextNS<coord_t>;
  timer_start();

  timer_next("Read map 0");
  LOG(INFO) << "Reading map 0 from " << config.map1_path;
  auto g1 = PlanarGraph<coord_t>::load_from(config.map1_path, config.serialize_prefix);

  timer_next("Read map 1");
  LOG(INFO) << "Reading map 1 from " << config.map2_path;
  auto g2 = PlanarGraph<coord_t>::load_from(config.map2_path, config.serialize_prefix);

  timer_next("Create App");
  context_t ctx({g1, g2});

  timer_next("Load Data");
  ctx.LoadToDevice();

  std::shared_ptr<MapOverlayNS<context_t>> overlay;

  if (config.mode == "rt") {
    auto overlay_rt = std::make_shared<MapOverlayRTNS<context_t>>(ctx);
    QueryConfigRT query_config;

    query_config.profile = config.profile;
    query_config.fau = config.fau;
    query_config.xsect_factor = config.xsect_factor;
    query_config.ag = config.ag;
    query_config.ag_iter = config.ag_iter;
    query_config.win = config.win;
    query_config.enlarge = config.enlarge;

    overlay_rt->set_config(query_config);
    overlay = overlay_rt;
  } else if (config.mode == "grid") {
    // auto overlay_grid = std::make_shared<MapOverlayGrid<context_t>>(ctx);
    // QueryConfigGrid query_config;
    //
    // query_config.grid_size = config.grid_size;
    // query_config.profile = config.profile;
    // query_config.xsect_factor = config.xsect_factor;
    //
    // overlay_grid->set_config(query_config);
    // overlay = overlay_grid;
  } else if (config.mode == "lbvh") {
    // auto overlay_lbvh = std::make_shared<MapOverlayLBVH<context_t>>(ctx);
    // QueryConfigLBVH query_config;
    //
    // query_config.profile = config.profile;
    // query_config.xsect_factor = config.xsect_factor;
    //
    // overlay_lbvh->set_config(query_config);
    // overlay = overlay_lbvh;
  } else {
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
}  // namespace vk
}  // namespace rayjoin
