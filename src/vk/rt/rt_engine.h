#ifndef RAYJOIN_RT_ENGINE_H
#define RAYJOIN_RT_ENGINE_H

#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>

#include "vk/engine/vk_buffer.h"

namespace rayjoin {
namespace vk {
class RTEngine {
public:
  RTEngine();
  ~RTEngine();

  // --------------------------------------------------------------------------
  // EXISTING:
  // Must be called once before BLAS build / RT pipeline creation.
  // --------------------------------------------------------------------------
  void Init();

  // --------------------------------------------------------------------------
  // EXISTING:
  // Equivalent to OptiX BuildAccelCustom().
  // Builds a BLAS over AABB primitives and returns the compacted AS handle.
  // --------------------------------------------------------------------------
  VkAccelerationStructureKHR BuildAccelCustom(const VkDeviceBuf &aabb_buf, uint32_t primitive_count);

  // ==========================================================================
  // NEWLY ADDED FOR LSI RAY TRACING
  // ==========================================================================

  // --------------------------------------------------------------------------
  // NEW:
  // Initializes the Vulkan ray tracing pipeline used by LSI.
  //
  // Expected shaders:
  //   - raygen shader
  //   - intersection shader
  //   - miss shader
  //
  // This is the Vulkan replacement for OptiX module/pipeline setup for LSI.
  // --------------------------------------------------------------------------
  void InitLSIPipeline(const char *rgen_spv, const char *rint_spv, const char *rmiss_spv);

  // --------------------------------------------------------------------------
  // NEW:
  // Stores all per-query resources needed by the LSI ray tracing launch.
  //
  // This is the Vulkan equivalent of preparing OptiX launch params.
  // --------------------------------------------------------------------------
  void SetLSIQuery(VkAccelerationStructureKHR handle,
                   const VkDeviceBuf &eid_range_buf,
                   const VkDeviceBuf &base_points_buf,
                   const VkDeviceBuf &base_edges_buf,
                   const VkDeviceBuf &query_points_buf,
                   const VkDeviceBuf &query_edges_buf,
                   const VkDeviceBuf &scaling_buf,
                   const VkDeviceBuf &xsect_buf,
                   const VkDeviceBuf &xsect_counter_buf, // NEW
                   const VkDeviceBuf &prof_counter_buf,
                   uint32_t xsect_capacity,
                   int query_map_id,
                   uint32_t query_edge_count);

  // --------------------------------------------------------------------------
  // NEW:
  // Executes one LSI ray tracing query using the state previously set via
  // SetLSIQuery().
  // --------------------------------------------------------------------------
  void RunLSI();

private:
  // --------------------------------------------------------------------------
  // EXISTING:
  // Tracks BLAS handles and their backing buffers so they can be destroyed
  // safely when the engine goes away.
  // --------------------------------------------------------------------------
  // struct AccelEntry {
  //   VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
  //   VkDeviceBuf buffer;
  // };
  struct AccelEntry {
    VkAccelerationStructureKHR accel = VK_NULL_HANDLE; // TLAS
    VkDeviceBuf buffer; // TLAS buffer

    VkAccelerationStructureKHR blas = VK_NULL_HANDLE; // BLAS
    VkDeviceBuf blasBuffer; // BLAS buffer

    VkDeviceBuf instanceBuffer; // TLAS instance data
  };

  // ==========================================================================
  // NEWLY ADDED FOR LSI QUERY STATE
  // ==========================================================================

  // --------------------------------------------------------------------------
  // NEW:
  // Cached per-dispatch resources for one LSI query.
  // These are the Vulkan equivalent of the OptiX launch parameter payload.
  // --------------------------------------------------------------------------
  struct LSIQueryState {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;

    const VkDeviceBuf *eid_range_buf = nullptr;
    const VkDeviceBuf *base_points_buf = nullptr;
    const VkDeviceBuf *base_edges_buf = nullptr;
    const VkDeviceBuf *query_points_buf = nullptr;
    const VkDeviceBuf *query_edges_buf = nullptr;
    const VkDeviceBuf *scaling_buf = nullptr;
    const VkDeviceBuf *xsect_buf = nullptr;
    const VkDeviceBuf *xsect_counter_buf = nullptr;
    const VkDeviceBuf *prof_counter_buf = nullptr;

    uint32_t xsect_capacity = 0;
    int query_map_id = 0;
    uint32_t query_edge_count = 0;
  };

  // --------------------------------------------------------------------------
  // NEW:
  // GPU-side launch parameter buffer for the LSI ray tracing pipeline.
  // This buffer is updated every query, similar to OptiX CopyLaunchParams().
  // --------------------------------------------------------------------------
  VkDeviceBuf lsi_params_buf_;

  // --------------------------------------------------------------------------
  // NEW:
  // Ray tracing pipeline objects for the LSI path.
  // --------------------------------------------------------------------------
  VkPipeline lsi_pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout lsi_pipeline_layout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout lsi_desc_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool lsi_desc_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet lsi_desc_set_ = VK_NULL_HANDLE;

  // --------------------------------------------------------------------------
  // NEW:
  // Shader Binding Table storage for LSI.
  // A single buffer holds raygen / hit / miss records.
  // --------------------------------------------------------------------------
  VkDeviceBuf lsi_sbt_buf_;
  uint32_t lsi_sbt_stride_ = 0;

  // --------------------------------------------------------------------------
  // NEW:
  // Cached CPU-side query state for the next RunLSI().
  // --------------------------------------------------------------------------
  LSIQueryState lsi_query_{};

private:
  // --------------------------------------------------------------------------
  // EXISTING:
  // BLAS storage.
  // --------------------------------------------------------------------------
  std::vector<AccelEntry> accels_;

  const VkComputeContext *ctx_ = nullptr;
  VkDevice device_ = VK_NULL_HANDLE;

  // --------------------------------------------------------------------------
  // EXISTING:
  // Vulkan acceleration structure function pointers.
  // --------------------------------------------------------------------------
  PFN_vkCreateAccelerationStructureKHR fpCreateAccelerationStructureKHR = nullptr;

  PFN_vkDestroyAccelerationStructureKHR fpDestroyAccelerationStructureKHR = nullptr;

  PFN_vkGetAccelerationStructureBuildSizesKHR fpGetAccelerationStructureBuildSizesKHR = nullptr;

  PFN_vkCmdBuildAccelerationStructuresKHR fpCmdBuildAccelerationStructuresKHR = nullptr;

  PFN_vkCmdWriteAccelerationStructuresPropertiesKHR fpCmdWriteAccelerationStructuresPropertiesKHR = nullptr;

  PFN_vkCmdCopyAccelerationStructureKHR fpCmdCopyAccelerationStructureKHR = nullptr;

  // --------------------------------------------------------------------------
  // NEW:
  // Additional Vulkan RT function pointers needed for launching ray tracing
  // pipelines, creating SBTs, and obtaining AS device addresses.
  // --------------------------------------------------------------------------
  PFN_vkGetAccelerationStructureDeviceAddressKHR fpGetAccelerationStructureDeviceAddressKHR = nullptr;

  PFN_vkCreateRayTracingPipelinesKHR fpCreateRayTracingPipelinesKHR = nullptr;

  PFN_vkGetRayTracingShaderGroupHandlesKHR fpGetRayTracingShaderGroupHandlesKHR = nullptr;

  PFN_vkCmdTraceRaysKHR fpCmdTraceRaysKHR = nullptr;

private:
  // --------------------------------------------------------------------------
  // EXISTING + EXTENDED:
  // Loads Vulkan function pointers.
  // Previously only AS build functions were needed.
  // Now also loads ray tracing pipeline entry points.
  // --------------------------------------------------------------------------
  void loadFunctionPointers();

  // ==========================================================================
  // NEWLY ADDED FOR LSI PIPELINE SETUP
  // ==========================================================================

  // --------------------------------------------------------------------------
  // NEW:
  // Creates a shader module from a SPIR-V file.
  // Used by InitLSIPipeline().
  // --------------------------------------------------------------------------
  VkShaderModule loadShaderModule(const char *spv_path);

  // --------------------------------------------------------------------------
  // NEW:
  // Creates descriptor set layout for LSI pipeline resources.
  // --------------------------------------------------------------------------
  void createLSIDescriptorSetLayout();

  // --------------------------------------------------------------------------
  // NEW:
  // Creates descriptor pool and descriptor set for LSI pipeline.
  // --------------------------------------------------------------------------
  void createLSIDescriptorPool();
  void allocateLSIDescriptorSet();

  // --------------------------------------------------------------------------
  // NEW:
  // Creates pipeline layout and ray tracing pipeline for LSI shaders.
  // --------------------------------------------------------------------------
  void createLSIPipelineLayout();
  void createLSIRTPipeline(const char *rgen_spv, const char *rint_spv, const char *rmiss_spv);

  // --------------------------------------------------------------------------
  // NEW:
  // Builds SBT data after the LSI RT pipeline is created.
  // --------------------------------------------------------------------------
  void buildLSISBT(uint32_t group_count);

  // --------------------------------------------------------------------------
  // NEW:
  // Updates descriptor bindings for the current LSI query.
  // Called from SetLSIQuery().
  // --------------------------------------------------------------------------
  void updateLSIDescriptors();

  // --------------------------------------------------------------------------
  // NEW:
  // Uploads CPU-side launch params into lsi_params_buf_.
  // Vulkan equivalent of OptiX CopyLaunchParams().
  // --------------------------------------------------------------------------
  template<typename T>
  void uploadLSIParams(const T &params) {
    if (lsi_params_buf_.Buf() == VK_NULL_HANDLE) {
      lsi_params_buf_.Init(sizeof(T));
    }

    VkStagingBuf staging(sizeof(T));
    std::vector<unsigned char> bytes(sizeof(T));
    std::memcpy(bytes.data(), &params, sizeof(T));

    staging.Host2Stage(bytes);
    staging.Stage2Device(lsi_params_buf_, sizeof(T));
  }
};

} // namespace vk
} // namespace rayjoin

#endif
