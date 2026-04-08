#ifndef RAYJOIN_LSI_RT_H
#define RAYJOIN_LSI_RT_H
#include "vk/map/_NOUSE_lsi_finalize_pass.h"

namespace rayjoin {
namespace vk {

template<typename CONTEXT_T>
class LSIRT : public LSI<CONTEXT_T> {
 public:
  // super class
  using lsi = LSI<CONTEXT_T>;

  explicit LSIRT(CONTEXT_T &ctx, const std::shared_ptr<RTEngine> &rt_engine) : LSI<CONTEXT_T>(ctx), rt_engine_(rt_engine) {}

  // Same as before
  void Init(size_t max_n_xsects) override { lsi::Init(max_n_xsects); }

  // use rt-engine
  void Query(int query_map_id) override {
    if (!rt_engine_) {
      throw std::runtime_error("LSIRT::Query(): rt_engine_ is null");
    }
    if (config_.handle == VK_NULL_HANDLE) {
      throw std::runtime_error("LSIRT::Query(): config_.handle is null");
    }
    if (config_.eid_range == nullptr) {
      throw std::runtime_error("LSIRT::Query(): config_.eid_range is null");
    }
    if (query_map_id != 0 && query_map_id != 1) {
      throw std::runtime_error("LSIRT::Query(): query_map_id must be 0 or 1");
    }

    auto &ctx = this->ctx_;
    int base_map_id = 1 - query_map_id;

    auto query_map = ctx.get_map(query_map_id);
    auto base_map = ctx.get_map(base_map_id);

    if (!query_map || !base_map) {
      throw std::runtime_error("LSIRT::Query(): map is null");
    }

    // This is the next interface you want RTEngine to support.
    // It wires together all resources needed by Vulkan RT traversal.
    rt_engine_->SetLSIQuery(config_.handle,
                            *config_.eid_range,
                            base_map->getPointsBuffer(),
                            base_map->getEdgesBuffer(),
                            query_map->getPointsBuffer(),
                            query_map->getEdgesBuffer(),
                            query_map->getScalingBuffer(),  // NEW
                            this->get_xsect_buffer(),
                            this->get_xsect_counter_buffer(),
                            this->get_prof_counter_buffer(),
                            static_cast<uint32_t>(this->get_xsect_capacity()),
                            query_map_id,
                            static_cast<uint32_t>(query_map->get_edges_num()));

    rt_engine_->RunLSI();


    // std::string spvPath = std::string(SHADER_DIR) + "/lsi_finalize.spv";
    std::string spvPath = std::string(SHADER_RT_NS_DIR) + "/lsi_finalize.spv";
    LSIFinalizePassRAII finalize_pass(spvPath.c_str(),  // [ADDED] compute shader SPIR-V
                                      rt_engine_->GetLSIParamsBuffer(),  // [ADDED] same LaunchParamsLSI buffer used by RT
                                      base_map->getEdgesBuffer(),  // [ADDED] binding: gBaseEdges
                                      base_map->getPointsBuffer(),  // [ADDED] binding: gBasePoints
                                      query_map->getEdgesBuffer(),  // [ADDED] binding: gQueryEdges
                                      query_map->getPointsBuffer(),  // [ADDED] binding: gQueryPoints
                                      this->get_xsect_buffer(),  // [ADDED] binding: gXsects
                                      this->get_xsect_counter_buffer(),  // [ADDED] binding: gXsectCounter
                                      static_cast<uint32_t>(this->get_xsect_capacity()));  // [ADDED]

    // [ADDED] Run compute pass after RT traversal.
    finalize_pass.run();
  }

  void set_config(QueryConfigRT config) { config_ = std::move(config); }

 private:
  std::shared_ptr<RTEngine> rt_engine_;
  QueryConfigRT config_;
  // SharedValue<uint32_t> n_tests_;
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_LSI_RT_H
