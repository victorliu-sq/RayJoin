# =========================== Shader =======================================
set(SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc)

# Source shader directories
set(SHADER_DIR_NS ${CMAKE_SOURCE_DIR}/src/vk/shader/kernel_ns)
set(SHADER_DIR_RT ${CMAKE_SOURCE_DIR}/src/vk/shader/rt)
set(SHADER_MODULE_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader/module)

# --------------------------------------------------------------------------
# Regular compute shaders: compile .slang files directly under SHADER_DIR_NS
# --------------------------------------------------------------------------
file(GLOB COMPUTE_SHADERS_NS
        ${SHADER_DIR_NS}/*.slang
)

set(SHADER_TARGETS_NS "")
set(SPIRV_OUTPUTS_NS "")

foreach (SHADER ${COMPUTE_SHADERS_NS})
  get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
  set(SPIRV ${SPV_OUTPUT_DIR_NS}/${SHADER_NAME}.spv)

  add_custom_command(
          OUTPUT ${SPIRV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_OUTPUT_DIR_NS}
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
# RT shader NS path: one source file, compiled 5 times into 5 SPIR-V files
#
# Source:
#   ${SHADER_DIR_RT}/lsi_rt_ns.slang
#
# Output:
#   ${SPV_OUTPUT_DIR_NS}/rt/*.spv
# --------------------------------------------------------------------------
set(RT_NS_SHADER ${SHADER_DIR_RT}/lsi_rt_ns.slang)
set(RT_NS_SPV_DIR ${SPV_OUTPUT_DIR_NS}/rt)

set(LSI_NS_RGEN_SPV ${RT_NS_SPV_DIR}/lsi_rgen_ns.spv)
set(LSI_NS_MISS_SPV ${RT_NS_SPV_DIR}/lsi_rmiss_ns.spv)
set(LSI_NS_RINT_SPV ${RT_NS_SPV_DIR}/lsi_rint_ns.spv)
set(LSI_NS_RAHIT_SPV ${RT_NS_SPV_DIR}/lsi_rahit_ns.spv)
set(LSI_NS_RCHIT_SPV ${RT_NS_SPV_DIR}/lsi_rchit_ns.spv)

add_custom_command(
        OUTPUT ${LSI_NS_RGEN_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_NS_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${RT_NS_SHADER}
        -entry raygenMain
        -stage raygeneration
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_NS_RGEN_SPV}
        DEPENDS ${RT_NS_SHADER}
        COMMENT "Compiling RT NS raygen shader lsi_rt_ns.slang -> lsi_rgen_ns.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_NS_MISS_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_NS_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${RT_NS_SHADER}
        -entry missMain
        -stage miss
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_NS_MISS_SPV}
        DEPENDS ${RT_NS_SHADER}
        COMMENT "Compiling RT NS miss shader lsi_rt_ns.slang -> lsi_rmiss_ns.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_NS_RINT_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_NS_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${RT_NS_SHADER}
        -entry intersectionMain
        -stage intersection
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_NS_RINT_SPV}
        DEPENDS ${RT_NS_SHADER}
        COMMENT "Compiling RT NS intersection shader lsi_rt_ns.slang -> lsi_rint_ns.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_NS_RAHIT_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_NS_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${RT_NS_SHADER}
        -entry anyHitMain
        -stage anyhit
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_NS_RAHIT_SPV}
        DEPENDS ${RT_NS_SHADER}
        COMMENT "Compiling RT NS any-hit shader lsi_rt_ns.slang -> lsi_rahit_ns.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_NS_RCHIT_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_NS_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${RT_NS_SHADER}
        -entry closestHitMain
        -stage closesthit
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_NS_RCHIT_SPV}
        DEPENDS ${RT_NS_SHADER}
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

# Optional global target
add_custom_target(shaders_ns ALL DEPENDS ${SPIRV_OUTPUTS_NS})

# =========================== LSI_VK Executable =============================
add_executable(polyover_vk_exec_ns main.cc)

target_link_libraries(polyover_vk_exec_ns PRIVATE
        rayjoin_lib_vk
)

add_dependencies(polyover_vk_exec_ns ${SHADER_TARGETS_NS})

# Pass shader output directory into the program as a macro.
# In C++:
#   std::string spvPath = std::string(SHADER_DIR_NS) + "/edge_init_ns_d64.spv";
#   std::string rgen    = std::string(SHADER_DIR_NS) + "/rt/lsi_rgen_ns.spv";