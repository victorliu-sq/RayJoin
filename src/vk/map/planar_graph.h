#ifndef RAYJOIN_PLANAR_GRAPH_H
#define RAYJOIN_PLANAR_GRAPH_H
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "glog/logging.h"
#include "shader/config.h"
#include "vk/map/bounding_box.h"
#include "vk/util/type_traits.h"

namespace rayjoin {
namespace vk {

struct Chain {
  int64_t id;  // chain index
  int64_t first_point_idx;  // unused, first, last index of the chain
  int64_t last_point_idx;
  int64_t left_polygon_id;  // left polygon id of the chain
  int64_t right_polygon_id;  // right polygon id of the chain
};

template<PointCoordType POINT_COORD_T>
struct PlanarGraph {
 public:
  using point_t = Vec2<POINT_COORD_T>;
  std::vector<Chain> chains;
  std::vector<index_t> row_index;  // organized in chains
  std::vector<point_t> points;
  BoundingBox<POINT_COORD_T> bb;

  static std::shared_ptr<PlanarGraph<POINT_COORD_T>> load_from(const std::string& path, const std::string& serialize_prefix) {
    std::string escaped_path;
    std::replace_copy(path.begin(), path.end(), std::back_inserter(escaped_path), '/', '-');
    // Ensure serialize directory exists (if requested)
    if (!serialize_prefix.empty()) {
      DIR* dir = opendir(serialize_prefix.c_str());
      if (dir) {
        closedir(dir);
      } else if (ENOENT == errno) {
        if (mkdir(serialize_prefix.c_str(), 0755)) {
          LOG(FATAL) << "Cannot create dir " << path;
        }
      } else {
        LOG(FATAL) << "Cannot open dir " << path;
      }
    }

    std::shared_ptr<PlanarGraph<POINT_COORD_T>> result;
    // auto ser_path = serialize_prefix + '/' + escaped_path + ".bin";
    const std::string ser_path = serialize_prefix + '/' + escaped_path + ".bin";

    // Load from cache if present; otherwise read from source and (optionally)
    // cache.
    const bool has_cache = (access(ser_path.c_str(), R_OK) == 0);
    if (has_cache) {
      // result = deserialize_pgraph<COORD_T>(ser_path.c_str());
    } else {
      result = read_pgraph(path.c_str());
      if (!serialize_prefix.empty() && access(serialize_prefix.c_str(), W_OK) == 0) {
        // serialize_pgraph(result, ser_path.c_str());
      }
    }
    return result;
  }

 private:
  static std::shared_ptr<PlanarGraph<POINT_COORD_T>> read_pgraph(const char* path) {
    std::ifstream ifs(path);

    CHECK(ifs.is_open()) << "Cannot open file " << path;

    std::string line;
    Chain* curr_chain;
    int64_t np = 0;
    auto pgraph = std::make_shared<PlanarGraph<POINT_COORD_T>>();
    auto& g = *pgraph;
    // typename cuda_vec<COORD_T>::type_2d* last_p = nullptr;
    Vec2<POINT_COORD_T>* last_p = nullptr;
    std::vector<double> seg_lens;
    size_t lno = 0;

    while (std::getline(ifs, line)) {
      lno++;
      if (line.empty() || line[0] == '#' || line[0] == '%') {
        continue;
      }
      std::istringstream iss(line);
      bool bad_line;

      if (np == 0) {
        g.chains.push_back(Chain());
        curr_chain = &g.chains.back();

        bad_line = !(iss >> curr_chain->id >> np >> curr_chain->first_point_idx >> curr_chain->last_point_idx >> curr_chain->left_polygon_id >>
                     curr_chain->right_polygon_id);
        bad_line |= np < 2;
        // checking overlapped polygon
        //      bad_line |= curr_chain->left_polygon_id ==
        //      curr_chain->right_polygon_id;
        pgraph->row_index.push_back(g.points.size());
        last_p = nullptr;
      } else {
        // typename cuda_vec<COORD_T>::type_2d p;
        Vec2<POINT_COORD_T> p;

        bad_line = !(iss >> p.x >> p.y);
        if (last_p != nullptr) {
          auto seg_len = sqrt((p.x - last_p->x) * (p.x - last_p->x) + (p.y - last_p->y) * (p.y - last_p->y));
          seg_lens.push_back(seg_len);
          bad_line |= p.x == last_p->x && p.y == last_p->y;
        }

        g.bb.min_x = std::min(g.bb.min_x, p.x);
        g.bb.max_x = std::max(g.bb.max_x, p.x);
        g.bb.min_y = std::min(g.bb.min_y, p.y);
        g.bb.max_y = std::max(g.bb.max_y, p.y);
        g.points.push_back(p);
        last_p = &g.points.back();
        np--;
      }

      CHECK(!bad_line) << "Bad line. Check your dataset! " << path << "[" << lno << "]: " << line;
    }
    ifs.close();

    if (!g.points.empty()) {  // in case of an empty graph
      pgraph->row_index.push_back(g.points.size());
    }
    CHECK_EQ(np, 0);

    double total_seg_len = std::accumulate(seg_lens.begin(), seg_lens.end(), 0.0);
    double mean = total_seg_len / seg_lens.size();

    std::vector<double> diff(seg_lens.size());
    std::transform(seg_lens.begin(), seg_lens.end(), diff.begin(), [mean](double x) { return x - mean; });
    double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    double stdev = std::sqrt(sq_sum / seg_lens.size());

    VLOG(1) << "Map " << path << " is loaded, chains: " << g.chains.size() << " points: " << pgraph->points.size()
            << " edges: " << g.points.size() - g.chains.size() << ", min seg len: " << *std::min_element(seg_lens.begin(), seg_lens.end())
            << ", max seg len: " << *std::max_element(seg_lens.begin(), seg_lens.end()) << ", avg seg len: " << mean << ", stdev: " << stdev;
    return pgraph;
  }
};

}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_PLANAR_GRAPH_H
