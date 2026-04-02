#ifndef RAYJOIN_MAP_OVERLAY_RT_H
#define RAYJOIN_MAP_OVERLAY_RT_H

#include "../engine/vk_buffer_readback.h"
#include "vk/core/lsi_rt.h"
#include "vk/engine/vk_buffer.h"
#include "vk/map/map.h"
#include "vk/rt/primitives.h"
#include "vk/rt/rt_engine.h"

namespace rayjoin {
namespace vk {

using index_t = rayjoin::index_t;
using polygon_id_t = rayjoin::polygon_id_t;

struct alignas(16) Int128 {
  uint64_t lo;
  int64_t hi;
};

struct alignas(16) Rational128 {
  Int128 num;
  Int128 den;
};

struct alignas(16) Intersection128 {
  Rational128 x;
  Rational128 y;
  index_t eid0;
  index_t eid1;
  polygon_id_t mid_point_polygon_id;
  uint32_t pad;
};

static_assert(sizeof(Int128) == 16);
static_assert(alignof(Int128) == 16);
static_assert(sizeof(Rational128) == 32);
static_assert(alignof(Rational128) == 16);
static_assert(sizeof(Intersection128) == 80);
static_assert(alignof(Intersection128) == 16);
static_assert(std::is_trivially_copyable_v<Intersection128>);

template<typename CONTEXT_T>
class MapOverlayRT : public MapOverlay<CONTEXT_T> {
  using map_t = typename CONTEXT_T::map_t;

 public:
  explicit MapOverlayRT(CONTEXT_T &ctx) : MapOverlay<CONTEXT_T>(ctx) {}

  void set_config(const QueryConfigRT &config) { config_ = config; }

  void Init() override {
    auto &ctx = this->ctx_;
    // -------------------------------
    // Scan maps
    // -------------------------------
    size_t max_n_points = 0;
    size_t max_n_edges = 0;

    for (int im = 0; im < 2; im++) {
      std::shared_ptr<map_t> map = ctx.get_map(im);

      size_t np = map->get_points_num();
      size_t ne = map->get_edges_num();

      map_point_count_[im] = np;
      map_edge_count_[im] = ne;
      LOG(INFO) << "Map-" << im << ": points=" << np << ", edges=" << ne;

      // closest edge id per vertex
      closest_eids_buf_[im].Init(sizeof(index_t) * np);

      // point -> polygon id
      point_in_polygon_buf_[im].Init(sizeof(index_t) * np);

      // primitive -> edge range mapping
      eid_range_buf_[im].Init(sizeof(EidRange) * ne);

      max_n_points = std::max(max_n_points, np);
      max_n_edges = std::max(max_n_edges, ne);
    }

    // -------------------------------
    // Allocate AABB primitive buffer
    // -------------------------------
    aabbs_buf_.Init(sizeof(VkAabbPositionsKHR) * max_n_edges);

    size_t n_edges = map_edge_count_[0] + map_edge_count_[1];
    xsect_capacity_ = static_cast<uint32_t>(n_edges * config_.xsect_factor);

    xsect_buf_.Init(sizeof(Intersection128) * xsect_capacity_);
    xsect_counter_buf_.Init(sizeof(uint32_t));
    prof_counter_buf_.Init(sizeof(uint32_t) * 16);
  }

  void BuildIndex() override {
    auto &ctx = this->ctx_;

    struct FillPrimitivesParams {
      uint32_t numEdges;
      uint32_t maxIter;
      float areaEnlarge;
      uint32_t pad;
    };

    static_assert(std::is_trivially_copyable_v<FillPrimitivesParams>);
    static_assert(sizeof(FillPrimitivesParams) == 16);

    const bool dump_index = rayjoin::ShouldDumpStage(config_.dump_results, "index");
    const std::string index_dir = rayjoin::DumpSubdir(config_.dump_dir, "results_index");

    for (int im = 0; im < 2; ++im) {
      auto map = ctx.get_map(im);

      // std::string spvPath = std::string(SHADER_DIR) + "/fill_primitives.spv";
      std::string spvPath = std::string(SHADER_KERNEL_DIR) + "/fill_primitives.spv";

      FillPrimitivesParams params{};
      params.numEdges = static_cast<uint32_t>(map_edge_count_[im]);
      params.maxIter = static_cast<uint32_t>(ROUNDING_ITER);
      params.areaEnlarge = config_.enlarge;
      params.pad = 0;

      RunComputePass(static_cast<uint32_t>(map_edge_count_[im]),
                     spvPath.c_str(),
                     params,
                     map->getPointsBuffer(),
                     map->getEdgesBuffer(),
                     aabbs_buf_,
                     eid_range_buf_[im],
                     map->getScalingBuffer());

      LOG(INFO) << "Map-" << im << " builds " << map_edge_count_[im] << " primtives.";

      if (dump_index) {
        DumpIndexResultsCSV(im, index_dir, "vulkan");
      }

      accel_[im].BuildAccelCustom(aabbs_buf_, static_cast<uint32_t>(map_edge_count_[im]));
      traverse_handles_[im] = accel_[im].GetTraverseHandle();
    }
  }

  void IntersectEdge(int query_map_id) override {
    const int base_map_id = 1 - query_map_id;

    auto query_map = this->ctx_.get_map(query_map_id);
    auto base_map = this->ctx_.get_map(base_map_id);

    if (!query_map || !base_map) {
      throw std::runtime_error("IntersectEdge(): null map");
    }

    {
      uint32_t zero = 0;
      writeToStorageBuffer(xsect_counter_buf_, zero);

      std::vector<uint32_t> zeros(16, 0u);
      writeToStorageBuffer(prof_counter_buf_, zeros);
    }

    // std::string rgen_spv = std::string(SHADER_DIR) + "/rt/lsi_rgen.spv";
    // std::string rint_spv = std::string(SHADER_DIR) + "/rt/lsi_rint.spv";
    // std::string rahit_spv = std::string(SHADER_DIR) + "/rt/lsi_rahit.spv";
    // std::string rchit_spv = std::string(SHADER_DIR) + "/rt/lsi_rchit.spv";
    // std::string rmiss_spv = std::string(SHADER_DIR) + "/rt/lsi_rmiss.spv";
    std::string rgen_spv = std::string(SHADER_RT_DIR) + "/lsi_rgen.spv";
    std::string rint_spv = std::string(SHADER_RT_DIR) + "/lsi_rint.spv";
    std::string rahit_spv = std::string(SHADER_RT_DIR) + "/lsi_rahit.spv";
    std::string rchit_spv = std::string(SHADER_RT_DIR) + "/lsi_rchit.spv";
    std::string rmiss_spv = std::string(SHADER_RT_DIR) + "/lsi_rmiss.spv";

    struct LaunchParamsLSI {
      int32_t query_map_id;
      uint32_t query_edge_count;
      uint32_t xsect_capacity;
      uint32_t pad0;
    };

    static_assert(std::is_trivially_copyable_v<LaunchParamsLSI>);
    static_assert(sizeof(LaunchParamsLSI) == 16);

    RunRTPass(rgen_spv.c_str(),
              rint_spv.c_str(),
              rahit_spv.c_str(),
              rchit_spv.c_str(),
              rmiss_spv.c_str(),
              accel_[base_map_id].GetTraverseHandle(),
              LaunchParamsLSI{
                  .query_map_id = static_cast<int32_t>(query_map_id),
                  .query_edge_count = static_cast<uint32_t>(query_map->get_edges_num()),
                  .xsect_capacity = xsect_capacity_,
                  .pad0 = 0u,
              },
              static_cast<uint32_t>(query_map->get_edges_num()),
              base_map->getEdgesBuffer(),  // binding 1
              base_map->getPointsBuffer(),  // binding 2
              eid_range_buf_[base_map_id],  // binding 3
              query_map->getEdgesBuffer(),  // binding 4
              query_map->getPointsBuffer(),  // binding 5
              query_map->getScalingBuffer(),  // binding 6
              xsect_buf_,  // binding 7
              xsect_counter_buf_,  // binding 8
              prof_counter_buf_);  // binding 9

    // std::string finalize_spv = std::string(SHADER_DIR) + "/lsi_finalize.spv";
    std::string finalize_spv = std::string(SHADER_KERNEL_DIR) + "/lsi_finalize.spv";

    struct LaunchParamsLSIFinalize {
      int32_t query_map_id;
      uint32_t query_edge_count;
      uint32_t xsect_capacity;
      uint32_t pad0;
    };

    static_assert(std::is_trivially_copyable_v<LaunchParamsLSIFinalize>);
    static_assert(sizeof(LaunchParamsLSIFinalize) == 16);

    RunComputePass(xsect_capacity_,
                   finalize_spv.c_str(),
                   LaunchParamsLSIFinalize{
                       .query_map_id = query_map_id,
                       .query_edge_count = static_cast<uint32_t>(query_map->get_edges_num()),
                       .xsect_capacity = xsect_capacity_,
                       .pad0 = 0u,
                   },
                   base_map->getEdgesBuffer(),  // binding 0
                   base_map->getPointsBuffer(),  // binding 1
                   query_map->getEdgesBuffer(),  // binding 2
                   query_map->getPointsBuffer(),  // binding 3
                   xsect_buf_,  // binding 4
                   xsect_counter_buf_);  // binding 5

    if (query_map_id == 0 && rayjoin::ShouldDumpStage(config_.dump_results, "lsi")) {
      DumpLSIResultsCSV(rayjoin::DumpSubdir(config_.dump_dir, "results_lsi"), "vulkan");
    }
  }

  void LocateVerticesInOtherMap(int query_map_id) override {}

  void ComputeOutputPolygons() override {}

  void WriteResult(const char *path) override {}

 private:
  QueryConfigRT config_;

  // -------------------------------
  // GPU buffers
  // -------------------------------
  // per-map: vertex -> closest edge
  VkDeviceBuf closest_eids_buf_[2];
  // per-map: vertex -> polygon id
  VkDeviceBuf point_in_polygon_buf_[2];
  // per-map: primitive -> edge index range
  VkDeviceBuf eid_range_buf_[2];
  // AABB primitives used for RT acceleration structure
  VkDeviceBuf aabbs_buf_;
  // -------------------------------
  // cached map sizes
  // -------------------------------
  size_t map_point_count_[2] = {0, 0};
  size_t map_edge_count_[2] = {0, 0};


  AccelStructScene accel_[2];

  // -------------------------------
  // BVH Handlers
  // -------------------------------
  VkAccelerationStructureKHR traverse_handles_[2];
  // Queue<xsect_t> xsect_queue_;


  // -------------------------------
  // LSI
  // -------------------------------
  VkDeviceBuf xsect_buf_;
  VkDeviceBuf xsect_counter_buf_;
  VkDeviceBuf prof_counter_buf_;
  uint32_t xsect_capacity_ = 0;

  void DumpIndexResultsCSV(int map_id, const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    const uint32_t n = static_cast<uint32_t>(map_edge_count_[map_id]);

    auto h_aabbs = readBackStorageBuffer<VkAabbPositionsKHR>(aabbs_buf_, n);
    auto h_ranges = readBackStorageBuffer<EidRange>(eid_range_buf_[map_id], n);

    if (h_aabbs.size() != n || h_ranges.size() != n) {
      LOG(ERROR) << "DumpIndexResultsCSV: readback size mismatch for map_id=" << map_id << " aabbs=" << h_aabbs.size()
                 << " ranges=" << h_ranges.size() << " expected=" << n;
      return;
    }

    const std::string path = out_dir + "/" + impl_tag + "_index_map_" + std::to_string(map_id) + ".csv";

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpIndexResultsCSV: failed to open " << path;
      return;
    }

    ofs << "map_id,primitive_id,min_x,min_y,min_z,max_x,max_y,max_z,eid_begin,eid_end\n";
    ofs << std::fixed << std::setprecision(9);

    for (uint32_t i = 0; i < n; ++i) {
      const auto &a = h_aabbs[i];
      const auto &r = h_ranges[i];

      ofs << map_id << "," << i << "," << a.minX << "," << a.minY << "," << a.minZ << "," << a.maxX << "," << a.maxY << "," << a.maxZ << ","
          << r.first << "," << r.second << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpIndexResultsCSV: wrote " << path;
  }

  void DumpLSIResultsCSV(const std::string &out_dir, const std::string &impl_tag) const {
    namespace fs = std::filesystem;
    fs::create_directories(out_dir);

    uint32_t n_xsects = 0;
    {
      auto h_counter = readBackStorageBuffer<uint32_t>(xsect_counter_buf_, 1);
      if (h_counter.empty()) {
        LOG(ERROR) << "DumpLSIResultsCSV: failed to read xsect counter";
        return;
      }
      n_xsects = std::min<uint32_t>(h_counter[0], xsect_capacity_);
    }

    auto h_xsects = readBackStorageBuffer<Intersection128>(xsect_buf_, n_xsects);

    const std::string path = out_dir + "/" + impl_tag + "_lsi.csv";

    std::vector<std::pair<uint64_t, uint64_t>> pairs;
    pairs.reserve(h_xsects.size());

    for (const auto &x: h_xsects) {
      uint64_t eid1 = static_cast<uint64_t>(x.eid0);
      uint64_t eid2 = static_cast<uint64_t>(x.eid1);
      if (eid1 > eid2) std::swap(eid1, eid2);
      pairs.emplace_back(eid1, eid2);
    }

    std::sort(pairs.begin(), pairs.end());

    std::ofstream ofs(path);
    if (!ofs) {
      LOG(ERROR) << "DumpLSIResultsCSV: failed to open " << path;
      return;
    }

    ofs << "eid1,eid2\n";
    for (const auto &[eid1, eid2]: pairs) {
      ofs << eid1 << "," << eid2 << "\n";
    }

    ofs.close();
    LOG(INFO) << "DumpLSIResultsCSV: wrote " << path;
  }
};

}  // namespace vk
}  // namespace rayjoin
#endif  // RAYJOIN_MAP_OVERLAY_RT_H
