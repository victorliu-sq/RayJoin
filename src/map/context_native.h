#ifndef RAYJOIN_CONTEXT_NS_H
#define RAYJOIN_CONTEXT_NS_H

#include <filesystem>
#include <libgen.h>

#include "core/query_config.h"
#include "core/run_query.h"
#include "map/map_native.h"
#include "map/planar_graph.h"
#include "util/dump.h"

namespace rayjoin {
template<typename COORD_T>
class ContextNative {
 public:
  using coord_t = COORD_T;
  using planar_graph_t = PlanarGraph<coord_t>;
  using map_t = MapNative<coord_t>;
  using bounding_box_t = BoundingBox<coord_t>;

  ContextNative() = default;
  ContextNative(const map_t&) = delete;
  ContextNative& operator=(const map_t&) = delete;

  explicit ContextNative(const std::shared_ptr<planar_graph_t>& pgraph) : ContextNative(std::array<std::shared_ptr<planar_graph_t>, 2>{pgraph}) {}

  explicit ContextNative(const std::array<std::shared_ptr<planar_graph_t>, 2>& planar_graphs) : planar_graphs_(planar_graphs) {
    for (auto pgraph: planar_graphs_) {
      if (pgraph != nullptr) {
        auto& bb = pgraph->bb;
        bb_.min_x = std::min(bb_.min_x, bb.min_x);
        bb_.max_x = std::max(bb_.max_x, bb.max_x);
        bb_.min_y = std::min(bb_.min_y, bb.min_y);
        bb_.max_y = std::max(bb_.max_y, bb.max_y);
      }
    }

    LOG(INFO) << "Bounding Box, Bottom-left: (" << bb_.min_x << ", " << bb_.min_y << "), Top-right: (" << bb_.max_x << ", " << bb_.max_y << ")";

    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    const char* path = "";
    if (count != -1) {
      path = dirname(result);
    }
    exec_root = std::string(path);
  }

  void LoadToDevice() {
    for (size_t im = 0; im < planar_graphs_.size(); ++im) {
      auto pgraph = planar_graphs_[im];
      if (pgraph == nullptr) {
        LOG(WARNING) << "LoadToDevice: planar_graphs_[" << im << "] is null";
        continue;
      }

      auto map = std::make_shared<map_t>(static_cast<int>(im));
      map->LoadFrom(stream_, *pgraph);
      maps_[im] = map;
    }
  }

  void LoadToDevice(const QueryConfigRT& query_config) {
    const bool dump_map = rayjoin::ShouldDumpStage(query_config.dump_results, "map");
    const std::string points_dir = rayjoin::DumpSubdir(query_config.dump_dir, "results_points");
    const std::string edges_dir = rayjoin::DumpSubdir(query_config.dump_dir, "results_edges");

    for (size_t im = 0; im < planar_graphs_.size(); ++im) {
      auto pgraph = planar_graphs_[im];
      if (pgraph == nullptr) {
        LOG(WARNING) << "LoadToDevice: planar_graphs_[" << im << "] is null";
        continue;
      }

      auto map = std::make_shared<map_t>(static_cast<int>(im));
      map->LoadFrom(stream_, *pgraph);

      if (dump_map) {
        map->DumpPointsCSV(points_dir, "optix_ns");
        map->DumpEdgesCSV(edges_dir, "optix_ns");
      }

      maps_[im] = map;
    }
  }

  void set_query_map(std::shared_ptr<map_t> query_map) { maps_[1] = query_map; }
  void set_map(int mapno, std::shared_ptr<map_t> map) { maps_[mapno] = map; }

  std::shared_ptr<map_t> get_map(int mapno) { return maps_[mapno]; }
  std::shared_ptr<const map_t> get_map(int mapno) const { return maps_.at(mapno); }

  std::shared_ptr<planar_graph_t> get_planar_graph(int mapno) { return planar_graphs_[mapno]; }
  std::shared_ptr<const planar_graph_t> get_planar_graph(int mapno) const { return planar_graphs_.at(mapno); }

  size_t get_maps_num() const { return maps_.size(); }

  const bounding_box_t& get_bounding_box() const { return bb_; }

  Stream& get_stream() { return stream_; }

  const std::string& get_exec_root() const { return exec_root; }

 private:
  Stream stream_;
  std::array<std::shared_ptr<planar_graph_t>, 2> planar_graphs_;
  std::array<std::shared_ptr<map_t>, 2> maps_;

  bounding_box_t bb_;
  std::string exec_root;
};

}  // namespace rayjoin


#endif  // RAYJOIN_CONTEXT_NS_H
