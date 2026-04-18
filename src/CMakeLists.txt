# =============================================================================
# src/CMakeLists.txt
#
# Build policy:
#   - If RAYJOIN_BUILD_OPTIX=ON: build OptiX/CUDA backend and apps
#   - If RAYJOIN_BUILD_OPTIX=OFF: skip all OptiX/CUDA targets
#   - Vulkan backend remains independent and can still build
# =============================================================================

# Shared include path used by both backends
include_directories(${PROJECT_SOURCE_DIR}/src)

# =============================================================================
# OptiX / CUDA backend
# =============================================================================
if (RAYJOIN_BUILD_OPTIX)

  # =========================== PTX Shaders ===================================
  # PTX compilation settings for OptiX device code.
  set(OPTIX_MODULE_EXTENSION ".ptx")
  set(OPTIX_PROGRAM_TARGET "--ptx")
  set(ENABLED_ARCHS "86")

  set(SHADERS
          shader/rt_lsi_custom.cu
          shader/rt_lsi_custom_native.cu
          shader/rt_pip_custom.cu
          shader/rt_pip_custom_native.cu
  )

  NVCUDA_COMPILE_MODULE(
          SOURCES ${SHADERS}
          DEPENDENCIES ${SHADERS_HEADERS}
          TARGET_PATH "${MODULE_TARGET_DIR}/ptx"
          EXTENSION "${OPTIX_MODULE_EXTENSION}"
          GENERATED_FILES PROGRAM_MODULES
          NVCC_OPTIONS "${OPTIX_PROGRAM_TARGET}"
          "--gpu-architecture=compute_${ENABLED_ARCHS}"
          "--relocatable-device-code=true"
          "--expt-relaxed-constexpr"
          "-Wno-deprecated-gpu-targets"
          "--prec-div=true"
          "-I${OptiX_INCLUDE}"
          "-I${CMAKE_CURRENT_SOURCE_DIR}"
          "-DRAYJOIN_RT"
  )

  message(STATUS "PROGRAM_MODULES: ${PROGRAM_MODULES}")

  add_custom_target(optix_ptx ALL
          DEPENDS ${PROGRAM_MODULES}
  )

  # =========================== RayJoin-CUDA Library ==========================
  add_library(rayjoin_lib SHARED
          core/run_overlay.cu
          core/run_overlay_native.cu
          core/run_query.cu
          rt/rt_engine.cu
          flag/flags.cc
          ${PROGRAM_MODULES}
  )

  target_link_libraries(rayjoin_lib PUBLIC
          CUDA::cudart
          CUDA::cuda_driver
          glog::glog
  )

  target_include_directories(rayjoin_lib PUBLIC
          ${OptiX_INCLUDE}
          ${PROJECT_SOURCE_DIR}/deps/Declib
          ${PROJECT_SOURCE_DIR}/deps/lbvh
          ${PROJECT_SOURCE_DIR}/src
  )

  target_compile_options(rayjoin_lib PRIVATE
          $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
  )

  set_target_properties(rayjoin_lib PROPERTIES
          CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
          CUDA_SEPARABLE_COMPILATION ON
  )

  add_dependencies(rayjoin_lib optix_ptx)

endif ()

# =============================================================================
# Vulkan backend
# =============================================================================
if (RAYJOIN_BUILD_VULKAN)

  # =========================== Vk Shader =====================================
  set(SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc)

  # Source shader directories
  set(SHADER_DIR_NS ${CMAKE_SOURCE_DIR}/src/vk/shader/kernel_ns)
  set(SHADER_RT_NS_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader/rt_ns)
  set(SHADER_MODULE_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader/module)

  # Output SPIR-V directories under build tree.
  set(SPV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
  set(SPV_OUTPUT_DIR_NS "${CMAKE_BINARY_DIR}/shaders_ns")

  set(SPV_KERNEL_DIR "${SPV_OUTPUT_DIR}")
  set(SPV_RT_DIR "${SPV_OUTPUT_DIR}/rt")

  set(SPV_KERNEL_NS_DIR "${SPV_OUTPUT_DIR_NS}")
  set(SPV_RT_NS_DIR "${SPV_OUTPUT_DIR_NS}/rt")

  # --------------------------------------------------------------------------
  # Compute shaders
  # --------------------------------------------------------------------------
  file(GLOB COMPUTE_SHADERS_NS
          ${SHADER_DIR_NS}/*.slang
  )

  set(SHADER_TARGETS_NS "")
  set(SPIRV_OUTPUTS_NS "")

  foreach (SHADER ${COMPUTE_SHADERS_NS})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    set(SPIRV ${SPV_KERNEL_NS_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_KERNEL_NS_DIR}
            COMMAND ${SLANGC}
            -I ${SHADER_MODULE_DIR}
            ${SHADER}
            -entry main
            -stage compute
            -profile sm_6_6
            -target spirv
            -fvk-use-scalar-layout
            -o ${SPIRV}
            DEPENDS ${SHADER}
            COMMENT "Compiling NS compute shader ${SHADER_NAME}.slang"
            VERBATIM
    )

    set(TARGET_NAME ${SHADER_NAME}_spv_ns)
    add_custom_target(${TARGET_NAME} DEPENDS ${SPIRV})

    list(APPEND SHADER_TARGETS_NS ${TARGET_NAME})
    list(APPEND SPIRV_OUTPUTS_NS ${SPIRV})
  endforeach ()

  # --------------------------------------------------------------------------
  # RT shader NS path: LSI
  # --------------------------------------------------------------------------
  set(LSI_RT_NS_SHADER ${SHADER_RT_NS_DIR}/lsi_rt_ns.slang)

  set(LSI_NS_RGEN_SPV ${SPV_RT_NS_DIR}/lsi_rgen_ns.spv)
  set(LSI_NS_MISS_SPV ${SPV_RT_NS_DIR}/lsi_rmiss_ns.spv)
  set(LSI_NS_RINT_SPV ${SPV_RT_NS_DIR}/lsi_rint_ns.spv)
  set(LSI_NS_RAHIT_SPV ${SPV_RT_NS_DIR}/lsi_rahit_ns.spv)
  set(LSI_NS_RCHIT_SPV ${SPV_RT_NS_DIR}/lsi_rchit_ns.spv)

  add_custom_command(
          OUTPUT ${LSI_NS_RGEN_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${LSI_RT_NS_SHADER}
          -entry raygenMain
          -stage raygeneration
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${LSI_NS_RGEN_SPV}
          DEPENDS ${LSI_RT_NS_SHADER}
          COMMENT "Compiling RT NS raygen shader lsi_rt_ns.slang -> lsi_rgen_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${LSI_NS_MISS_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${LSI_RT_NS_SHADER}
          -entry missMain
          -stage miss
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${LSI_NS_MISS_SPV}
          DEPENDS ${LSI_RT_NS_SHADER}
          COMMENT "Compiling RT NS miss shader lsi_rt_ns.slang -> lsi_rmiss_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${LSI_NS_RINT_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${LSI_RT_NS_SHADER}
          -entry intersectionMain
          -stage intersection
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${LSI_NS_RINT_SPV}
          DEPENDS ${LSI_RT_NS_SHADER}
          COMMENT "Compiling RT NS intersection shader lsi_rt_ns.slang -> lsi_rint_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${LSI_NS_RAHIT_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${LSI_RT_NS_SHADER}
          -entry anyHitMain
          -stage anyhit
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${LSI_NS_RAHIT_SPV}
          DEPENDS ${LSI_RT_NS_SHADER}
          COMMENT "Compiling RT NS any-hit shader lsi_rt_ns.slang -> lsi_rahit_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${LSI_NS_RCHIT_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${LSI_RT_NS_SHADER}
          -entry closestHitMain
          -stage closesthit
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${LSI_NS_RCHIT_SPV}
          DEPENDS ${LSI_RT_NS_SHADER}
          COMMENT "Compiling RT NS closest-hit shader lsi_rt_ns.slang -> lsi_rchit_ns.spv"
          VERBATIM
  )

  add_custom_target(lsi_rt_ns_rgen_spv DEPENDS ${LSI_NS_RGEN_SPV})
  add_custom_target(lsi_rt_ns_miss_spv DEPENDS ${LSI_NS_MISS_SPV})
  add_custom_target(lsi_rt_ns_rint_spv DEPENDS ${LSI_NS_RINT_SPV})
  add_custom_target(lsi_rt_ns_rahit_spv DEPENDS ${LSI_NS_RAHIT_SPV})
  add_custom_target(lsi_rt_ns_rchit_spv DEPENDS ${LSI_NS_RCHIT_SPV})

  list(APPEND SHADER_TARGETS_NS
          lsi_rt_ns_rgen_spv
          lsi_rt_ns_miss_spv
          lsi_rt_ns_rint_spv
          lsi_rt_ns_rahit_spv
          lsi_rt_ns_rchit_spv
  )

  list(APPEND SPIRV_OUTPUTS_NS
          ${LSI_NS_RGEN_SPV}
          ${LSI_NS_MISS_SPV}
          ${LSI_NS_RINT_SPV}
          ${LSI_NS_RAHIT_SPV}
          ${LSI_NS_RCHIT_SPV}
  )

  # --------------------------------------------------------------------------
  # RT shader NS path: PIP
  # --------------------------------------------------------------------------
  set(PIP_RT_NS_SHADER ${SHADER_RT_NS_DIR}/pip_rt_ns.slang)

  set(PIP_NS_RGEN_SPV ${SPV_RT_NS_DIR}/pip_rgen_ns.spv)
  set(PIP_NS_MISS_SPV ${SPV_RT_NS_DIR}/pip_rmiss_ns.spv)
  set(PIP_NS_RINT_SPV ${SPV_RT_NS_DIR}/pip_rint_ns.spv)
  set(PIP_NS_RAHIT_SPV ${SPV_RT_NS_DIR}/pip_rahit_ns.spv)
  set(PIP_NS_RCHIT_SPV ${SPV_RT_NS_DIR}/pip_rchit_ns.spv)

  add_custom_command(
          OUTPUT ${PIP_NS_RGEN_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${PIP_RT_NS_SHADER}
          -entry raygenMain
          -stage raygeneration
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${PIP_NS_RGEN_SPV}
          DEPENDS ${PIP_RT_NS_SHADER}
          COMMENT "Compiling RT NS raygen shader pip_rt_ns.slang -> pip_rgen_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${PIP_NS_MISS_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${PIP_RT_NS_SHADER}
          -entry missMain
          -stage miss
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${PIP_NS_MISS_SPV}
          DEPENDS ${PIP_RT_NS_SHADER}
          COMMENT "Compiling RT NS miss shader pip_rt_ns.slang -> pip_rmiss_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${PIP_NS_RINT_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${PIP_RT_NS_SHADER}
          -entry intersectionMain
          -stage intersection
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${PIP_NS_RINT_SPV}
          DEPENDS ${PIP_RT_NS_SHADER}
          COMMENT "Compiling RT NS intersection shader pip_rt_ns.slang -> pip_rint_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${PIP_NS_RAHIT_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${PIP_RT_NS_SHADER}
          -entry anyHitMain
          -stage anyhit
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${PIP_NS_RAHIT_SPV}
          DEPENDS ${PIP_RT_NS_SHADER}
          COMMENT "Compiling RT NS any-hit shader pip_rt_ns.slang -> pip_rahit_ns.spv"
          VERBATIM
  )

  add_custom_command(
          OUTPUT ${PIP_NS_RCHIT_SPV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_RT_NS_DIR}
          COMMAND ${SLANGC}
          -I ${SHADER_MODULE_DIR}
          ${PIP_RT_NS_SHADER}
          -entry closestHitMain
          -stage closesthit
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${PIP_NS_RCHIT_SPV}
          DEPENDS ${PIP_RT_NS_SHADER}
          COMMENT "Compiling RT NS closest-hit shader pip_rt_ns.slang -> pip_rchit_ns.spv"
          VERBATIM
  )

  add_custom_target(pip_rt_ns_rgen_spv DEPENDS ${PIP_NS_RGEN_SPV})
  add_custom_target(pip_rt_ns_miss_spv DEPENDS ${PIP_NS_MISS_SPV})
  add_custom_target(pip_rt_ns_rint_spv DEPENDS ${PIP_NS_RINT_SPV})
  add_custom_target(pip_rt_ns_rahit_spv DEPENDS ${PIP_NS_RAHIT_SPV})
  add_custom_target(pip_rt_ns_rchit_spv DEPENDS ${PIP_NS_RCHIT_SPV})

  list(APPEND SHADER_TARGETS_NS
          pip_rt_ns_rgen_spv
          pip_rt_ns_miss_spv
          pip_rt_ns_rint_spv
          pip_rt_ns_rahit_spv
          pip_rt_ns_rchit_spv
  )

  list(APPEND SPIRV_OUTPUTS_NS
          ${PIP_NS_RGEN_SPV}
          ${PIP_NS_MISS_SPV}
          ${PIP_NS_RINT_SPV}
          ${PIP_NS_RAHIT_SPV}
          ${PIP_NS_RCHIT_SPV}
  )

  add_custom_target(shaders_ns ALL DEPENDS ${SPIRV_OUTPUTS_NS})

  # =========================== RayJoin-VK ===================================
  set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Build all libs with -fPIC" FORCE)

  add_library(rayjoin_lib_vk SHARED
          flag/flags.cc
          vk/core/run_overlay_ns.cc
          vk/core/vk_global_context.cc
          vk/core/vma_impl.cc
  )

  target_include_directories(rayjoin_lib_vk PUBLIC
          ${PROJECT_SOURCE_DIR}/src
  )

  target_link_libraries(rayjoin_lib_vk PUBLIC
          glog::glog
          Vulkan::Vulkan
          GPUOpen::VulkanMemoryAllocator
  )

  target_compile_definitions(rayjoin_lib_vk PUBLIC
          SHADER_KERNEL_DIR="${SPV_KERNEL_DIR}"
          SHADER_RT_DIR="${SPV_RT_DIR}"
          SHADER_KERNEL_NS_DIR="${SPV_KERNEL_NS_DIR}"
          SHADER_RT_NS_DIR="${SPV_RT_NS_DIR}"
  )

  # IMPORTANT:
  # Vulkan target depends only on Vulkan shader outputs.
  # Do NOT depend on optix_ptx here.
  add_dependencies(rayjoin_lib_vk ${SHADER_TARGETS_NS})

endif ()

# =============================================================================
# Applications
# =============================================================================

# OptiX/CUDA apps
if (RAYJOIN_BUILD_OPTIX)
  add_subdirectory(app/01_overlay)
  add_subdirectory(app/02_query)
  add_subdirectory(app/04_overlay_native)
endif ()

# Vulkan app(s)
if (RAYJOIN_BUILD_VULKAN)
  add_subdirectory(app/03_overlay_vk_ns)
endif ()