#ifndef APP_OVERLAY_CONFIG_H
#define APP_OVERLAY_CONFIG_H

#include <string>

#include "flag/flags.h"
namespace rayjoin {

struct OverlayConfig {
  std::string exec_root;
  std::string map1_path;
  std::string map2_path;
  std::string serialize_prefix;
  std::string output_path;
  unsigned int grid_size;
  float xsect_factor;
  std::string mode;
  bool fau;
  bool check;
  bool profile;
  int win;
  int ag;
  int ag_iter;
  float enlarge;

  // dump
  std::string dump_results;
  std::string dump_dir;
};

inline OverlayConfig CreateOverlayConfig(const char* argv0) {
  OverlayConfig config;
  const std::string exec_path = argv0 ? argv0 : "";

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
  config.dump_results = FLAGS_dump_results;
  config.dump_dir = FLAGS_dump_dir;

  return config;
}
}  // namespace rayjoin

#endif  // APP_OVERLAY_CONFIG_H
