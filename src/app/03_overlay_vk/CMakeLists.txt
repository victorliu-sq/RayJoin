# =========================== Shader =======================================
## Common flags you already use (keep scalar layout for safer structs)
#set(SLANG_COMMON_FLAGS -profile sm_6_6 -target spirv -fvk-use-scalar-layout)
#
## Compile compute shader: scale_points_d2_i64.slang -> scale_points_d2_i64.spv
#slang_compile_spirv(
#        NAME scale_points
#        SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc
#        SOURCE ${CMAKE_SOURCE_DIR}/src/vk/shader/scale_points_d2_i64.slang
#        OUT_DIR ${SPV_OUTPUT_DIR}
#        FLAGS ${SLANG_COMMON_FLAGS}
#        ENTRIES
#        main compute scale_points_d2_i64.spv
#)
#
## Compile compute shader: edge_init_i64.slang -> edge_init_i64.spv
#slang_compile_spirv(
#        NAME edge_init_i64
#        SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc
#        SOURCE ${CMAKE_SOURCE_DIR}/src/vk/shader/edge_init_i64.slang
#        OUT_DIR ${SPV_OUTPUT_DIR}
#        FLAGS ${SLANG_COMMON_FLAGS}
#        ENTRIES
#        main compute edge_init_i64.spv
#)
#
#
## Compile compute shader: edge_init_i64_dev_addr.slang -> edge_init_i64_dev_addr.spv
#slang_compile_spirv(
#        NAME edge_init_i64_dev_addr
#        SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc
#        SOURCE ${CMAKE_SOURCE_DIR}/src/vk/shader/edge_init_i64_dev_addr.slang
#        OUT_DIR ${SPV_OUTPUT_DIR}
#        FLAGS ${SLANG_COMMON_FLAGS}
#        ENTRIES
#        main compute edge_init_i64_dev_addr.spv
#)

# =========================== Shader =======================================

set(SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc)
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader)

# --------------------------------------------------------------------------
# Regular compute shaders: only compile .slang files directly under SHADER_DIR
# Excludes SHADER_DIR/rt/*.slang, which are handled separately below.
# --------------------------------------------------------------------------
file(GLOB COMPUTE_SHADERS
        ${SHADER_DIR}/*.slang
)

set(SHADER_TARGETS "")
set(SPIRV_OUTPUTS "")

foreach (SHADER ${COMPUTE_SHADERS})
  get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
  set(SPIRV ${SPV_OUTPUT_DIR}/${SHADER_NAME}.spv)

  add_custom_command(
          OUTPUT ${SPIRV}
          COMMAND ${SLANGC}
          -I ${SHADER_DIR}/module
          ${SHADER}
          -entry main
          -stage compute
          -profile sm_6_6
          -target spirv
          -fvk-use-scalar-layout
          -o ${SPIRV}
          DEPENDS ${SHADER}
          COMMENT "Compiling compute shader ${SHADER_NAME}.slang"
          VERBATIM
  )

  set(TARGET_NAME ${SHADER_NAME}_spv)
  add_custom_target(${TARGET_NAME} DEPENDS ${SPIRV})

  list(APPEND SHADER_TARGETS ${TARGET_NAME})
  list(APPEND SPIRV_OUTPUTS ${SPIRV})
endforeach ()

# --------------------------------------------------------------------------
# RT shader: one source file, compiled 3 times into 3 SPIR-V files
#
# Assumes:
#   ${SHADER_DIR}/rt/lsi_rt.slang
#
# with entry points:
#   raygenMain        -> raygeneration
#   missMain          -> miss
#   intersectionMain  -> intersection
# --------------------------------------------------------------------------
set(RT_SHADER ${SHADER_DIR}/rt/lsi_rt.slang)
set(RT_SPV_DIR ${SPV_OUTPUT_DIR}/rt)

set(LSI_RGEN_SPV ${RT_SPV_DIR}/lsi_rgen.spv)
set(LSI_MISS_SPV ${RT_SPV_DIR}/lsi_miss.spv)
set(LSI_RINT_SPV ${RT_SPV_DIR}/lsi_rint.spv)

add_custom_command(
        OUTPUT ${LSI_RGEN_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_DIR}/module
        ${RT_SHADER}
        -entry raygenMain
        -stage raygeneration
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_RGEN_SPV}
        DEPENDS ${RT_SHADER}
        COMMENT "Compiling RT raygen shader lsi_rt.slang -> lsi_rgen.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_MISS_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_DIR}/module
        ${RT_SHADER}
        -entry missMain
        -stage miss
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_MISS_SPV}
        DEPENDS ${RT_SHADER}
        COMMENT "Compiling RT miss shader lsi_rt.slang -> lsi_miss.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_RINT_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_DIR}/module
        ${RT_SHADER}
        -entry intersectionMain
        -stage intersection
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_RINT_SPV}
        DEPENDS ${RT_SHADER}
        COMMENT "Compiling RT intersection shader lsi_rt.slang -> lsi_rint.spv"
        VERBATIM
)

add_custom_target(lsi_rt_rgen_spv DEPENDS ${LSI_RGEN_SPV})
add_custom_target(lsi_rt_miss_spv DEPENDS ${LSI_MISS_SPV})
add_custom_target(lsi_rt_rint_spv DEPENDS ${LSI_RINT_SPV})

list(APPEND SHADER_TARGETS
        lsi_rt_rgen_spv
        lsi_rt_miss_spv
        lsi_rt_rint_spv
)

list(APPEND SPIRV_OUTPUTS
        ${LSI_RGEN_SPV}
        ${LSI_MISS_SPV}
        ${LSI_RINT_SPV}
)

# Optional global target
add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# =========================== LSI_VK Executable =============================
add_executable(polyover_vk_exec main.cc)

target_link_libraries(polyover_vk_exec PRIVATE
        rayjoin_lib_vk
)

add_dependencies(polyover_vk_exec ${SHADER_TARGETS})