#ifndef RAYJOIN_APP_OUTPUT_CHAIN_NATIVE_H
#define RAYJOIN_APP_OUTPUT_CHAIN_NATIVE_H

#include <algorithm>
#include <fstream>
#include <map>
#include <thrust/device_vector.h>
#include <unordered_map>
#include <vector>

#include "glog/logging.h"
#include "shader/config.h"
#include "shader/lsi_native.h"
#include "util/type_traits.h"
#include "util/util.h"

namespace rayjoin {

template<typename COORD_T>
struct OutputChainNative {
  int64_t id;
  std::vector<typename cuda_vec<COORD_T>::type_2d> points;
  index_t first_point_idx;
  index_t last_point_idx;
  int64_t left_polygon_id;
  int64_t right_polygon_id;
  int64_t other_map_polygon_id;

  void AddChainPoint(const typename cuda_vec<COORD_T>::type_2d& p) { points.push_back(p); }

  void AddXsectPoint(const dev::IntersectionNative<COORD_T>& xsect) {
    typename cuda_vec<COORD_T>::type_2d p{xsect.x, xsect.y};
    points.push_back(p);
  }
};

template<typename CONTEXT_T>
void WriteOutputChainNative(CONTEXT_T& ctx,
                            const thrust::device_vector<dev::IntersectionNative<typename CONTEXT_T::coord_t>>* xsect_edges_sorted_pair,
                            const thrust::device_vector<index_t>* point_in_polygon_pair,
                            const char* path) {
  using coord_t = typename CONTEXT_T::coord_t;
  using xsect_t = dev::IntersectionNative<coord_t>;

  std::vector<OutputChainNative<coord_t>> output_chains;

  auto flush = [&output_chains](OutputChainNative<coord_t>& output_chain) {
    auto& points = output_chain.points;

    if (!points.empty()) {
      if (output_chain.left_polygon_id * output_chain.other_map_polygon_id != 0 ||
          output_chain.right_polygon_id * output_chain.other_map_polygon_id != 0) {
        auto p_it =
            std::unique(points.begin(), points.end(), [](const typename cuda_vec<coord_t>::type_2d& a, const typename cuda_vec<coord_t>::type_2d& b) {
              return a.x == b.x && a.y == b.y;
            });
        points.resize(std::distance(points.begin(), p_it));
        output_chain.id = output_chains.size();
        output_chains.push_back(output_chain);
        points.clear();
      }

      points.clear();
    }
  };

  pinned_vector<xsect_t> xsect_edges_sorted;
  pinned_vector<index_t> point_in_polygon;

  FOR2 {
    xsect_edges_sorted = xsect_edges_sorted_pair[im];
    point_in_polygon = point_in_polygon_pair[im];
    const auto& p_graph = *ctx.get_planar_graph(im);

    std::unordered_map<index_t, std::vector<xsect_t>> grouped_xsects;
    for (auto& xsect: xsect_edges_sorted) {
      grouped_xsects[xsect.eid[im]].push_back(xsect);
    }

    LOG(INFO) << "Map " << im << ", Xsect: " << xsect_edges_sorted.size() << " " << grouped_xsects.size();

    for (size_t ic = 0; ic < p_graph.chains.size(); ic++) {
      const auto& chain = p_graph.chains[ic];
      auto begin_pid = p_graph.row_index[ic];
      auto end_pid = p_graph.row_index[ic + 1];

      OutputChainNative<coord_t> output_chain;
      output_chain.left_polygon_id = chain.left_polygon_id;
      output_chain.right_polygon_id = chain.right_polygon_id;

      for (auto pid = begin_pid; pid < end_pid; pid++) {
        output_chain.other_map_polygon_id = point_in_polygon[pid];
        output_chain.AddChainPoint(p_graph.points[pid]);

        if (pid != end_pid - 1) {
          auto eid = pid - ic;
          auto it = grouped_xsects.find(eid);

          if (it != grouped_xsects.end()) {
            auto& xsects = it->second;

            if (!xsects.empty()) {
              output_chain.AddXsectPoint(xsects[0]);

              for (size_t ixsect = 0; ixsect < xsects.size() - 1; ixsect++) {
                flush(output_chain);

                const xsect_t& xsect = xsects[ixsect];
                const xsect_t& next_xsect = xsects[ixsect + 1];

                output_chain.other_map_polygon_id = xsect.mid_point_polygon_id;
                output_chain.AddXsectPoint(xsect);
                output_chain.AddXsectPoint(next_xsect);
              }

              flush(output_chain);
              output_chain.AddXsectPoint(xsects.back());
            }
          }
        }
      }

      flush(output_chain);
    }
  }

  std::map<std::pair<int64_t, int64_t>, size_t> face_ids;
  std::unordered_map<typename cuda_vec<coord_t>::type_2d, index_t> point_ids;
  index_t point_counter = 0;

  auto create_polygon = [&](int64_t polygon_id1, int64_t polygon_id2) -> size_t {
    if (polygon_id1 == 0 || polygon_id2 == 0) {
      return 0;
    }

    auto k = std::make_pair(polygon_id1, polygon_id2);
    auto it = face_ids.find(k);
    if (it == face_ids.end()) {
      face_ids[k] = face_ids.size() + 1;
      return face_ids.size();
    }
    return it->second;
  };

  // Jiaxin Patch: This version does not match original optix version
  // for (auto& chain: output_chains) {
  //   {
  //     const int64_t a = chain.left_polygon_id;
  //     const int64_t b = chain.other_map_polygon_id;
  //     chain.left_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
  //   }
  //
  //   {
  //     const int64_t a = chain.right_polygon_id;
  //     const int64_t b = chain.other_map_polygon_id;
  //     chain.right_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
  //   }
  //
  //   for (const auto& p: chain.points) {
  //     if (point_ids.find(p) == point_ids.end()) {
  //       point_ids[p] = point_counter++;
  //     }
  //   }
  //
  //   chain.first_point_idx = point_ids[chain.points.front()];
  //   chain.last_point_idx = point_ids[chain.points.back()];
  // }

  // Jiaxin Patch: This version does match original optix version
  for (auto& chain: output_chains) {
    if (chain.left_polygon_id < chain.other_map_polygon_id) {
      chain.left_polygon_id = create_polygon(chain.left_polygon_id, chain.other_map_polygon_id);
    } else {
      chain.left_polygon_id = create_polygon(chain.other_map_polygon_id, chain.left_polygon_id);
    }

    if (chain.right_polygon_id < chain.other_map_polygon_id) {
      chain.right_polygon_id = create_polygon(chain.right_polygon_id, chain.other_map_polygon_id);
    } else {
      chain.right_polygon_id = create_polygon(chain.other_map_polygon_id, chain.right_polygon_id);
    }

    {
      const int64_t a = chain.left_polygon_id;
      const int64_t b = chain.other_map_polygon_id;
      chain.left_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
    }

    {
      const int64_t a = chain.right_polygon_id;
      const int64_t b = chain.other_map_polygon_id;
      chain.right_polygon_id = (a <= b) ? create_polygon(a, b) : create_polygon(b, a);
    }

    for (const auto& p: chain.points) {
      if (point_ids.find(p) == point_ids.end()) {
        point_ids[p] = point_counter++;
      }
    }
    chain.first_point_idx = point_ids[chain.points.front()];
    chain.last_point_idx = point_ids[chain.points.back()];
  }

  LOG(INFO) << "Total chains: " << output_chains.size() << " Total faces: " << face_ids.size();

  std::ofstream ofs(path);
  CHECK(ofs.is_open()) << "Cannot open " << path;
  ofs.setf(std::ios::fixed, std::ios::floatfield);
  ofs.precision(6);

  for (size_t ichain = 0; ichain < output_chains.size(); ichain++) {
    const auto& chain = output_chains[ichain];
    ofs << (ichain + 1) << " " << chain.points.size() << " " << chain.first_point_idx << " " << chain.last_point_idx << " " << chain.left_polygon_id
        << " " << chain.right_polygon_id << '\n';

    for (const auto& p: chain.points) {
      ofs << p.x << " " << p.y << '\n';
    }
  }

  ofs.close();
}

}  // namespace rayjoin

#endif  // RAYJOIN_APP_OUTPUT_CHAIN_NATIVE_H
