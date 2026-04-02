# =========================== Shader =======================================
set(SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc)

# Source shader directories
set(SHADER_KERNEL_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader/kernel)
set(SHADER_RT_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader/rt)
set(SHADER_MODULE_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader/module)

# --------------------------------------------------------------------------
# Regular compute shaders: compile .slang files directly under SHADER_KERNEL_DIR
# --------------------------------------------------------------------------
file(GLOB COMPUTE_SHADERS
        ${SHADER_KERNEL_DIR}/*.slang
)

set(SHADER_TARGETS "")
set(SPIRV_OUTPUTS "")

foreach (SHADER ${COMPUTE_SHADERS})
  get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
  set(SPIRV ${SPV_OUTPUT_DIR}/${SHADER_NAME}.spv)

  add_custom_command(
          OUTPUT ${SPIRV}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_OUTPUT_DIR}
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
          COMMENT "Compiling compute shader ${SHADER_NAME}.slang"
          VERBATIM
  )

  set(TARGET_NAME ${SHADER_NAME}_spv)
  add_custom_target(${TARGET_NAME} DEPENDS ${SPIRV})

  list(APPEND SHADER_TARGETS ${TARGET_NAME})
  list(APPEND SPIRV_OUTPUTS ${SPIRV})
endforeach ()

# --------------------------------------------------------------------------
# RT shader path: scaling Vulkan
# --------------------------------------------------------------------------
set(LSI_RT_SHADER ${SHADER_RT_DIR}/lsi_rt.slang)
set(RT_SPV_DIR ${SPV_OUTPUT_DIR}/rt)

set(LSI_RGEN_SPV ${RT_SPV_DIR}/lsi_rgen.spv)
set(LSI_MISS_SPV ${RT_SPV_DIR}/lsi_rmiss.spv)
set(LSI_RINT_SPV ${RT_SPV_DIR}/lsi_rint.spv)
set(LSI_RAHIT_SPV ${RT_SPV_DIR}/lsi_rahit.spv)
set(LSI_RCHIT_SPV ${RT_SPV_DIR}/lsi_rchit.spv)

add_custom_command(
        OUTPUT ${LSI_RGEN_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${LSI_RT_SHADER}
        -entry raygenMain
        -stage raygeneration
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_RGEN_SPV}
        DEPENDS ${LSI_RT_SHADER}
        COMMENT "Compiling RT raygen shader lsi_rt.slang -> lsi_rgen.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_MISS_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${LSI_RT_SHADER}
        -entry missMain
        -stage miss
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_MISS_SPV}
        DEPENDS ${LSI_RT_SHADER}
        COMMENT "Compiling RT miss shader lsi_rt.slang -> lsi_rmiss.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_RINT_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${LSI_RT_SHADER}
        -entry intersectionMain
        -stage intersection
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_RINT_SPV}
        DEPENDS ${LSI_RT_SHADER}
        COMMENT "Compiling RT intersection shader lsi_rt.slang -> lsi_rint.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_RAHIT_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${LSI_RT_SHADER}
        -entry anyHitMain
        -stage anyhit
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_RAHIT_SPV}
        DEPENDS ${LSI_RT_SHADER}
        COMMENT "Compiling RT any-hit shader lsi_rt.slang -> lsi_rahit.spv"
        VERBATIM
)

add_custom_command(
        OUTPUT ${LSI_RCHIT_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RT_SPV_DIR}
        COMMAND ${SLANGC}
        -I ${SHADER_MODULE_DIR}
        ${LSI_RT_SHADER}
        -entry closestHitMain
        -stage closesthit
        -profile sm_6_6
        -target spirv
        -fvk-use-scalar-layout
        -o ${LSI_RCHIT_SPV}
        DEPENDS ${LSI_RT_SHADER}
        COMMENT "Compiling RT closest-hit shader lsi_rt.slang -> lsi_rchit.spv"
        VERBATIM
)

add_custom_target(lsi_rt_rgen_spv DEPENDS ${LSI_RGEN_SPV})
add_custom_target(lsi_rt_miss_spv DEPENDS ${LSI_MISS_SPV})
add_custom_target(lsi_rt_rint_spv DEPENDS ${LSI_RINT_SPV})
add_custom_target(lsi_rt_rahit_spv DEPENDS ${LSI_RAHIT_SPV})
add_custom_target(lsi_rt_rchit_spv DEPENDS ${LSI_RCHIT_SPV})

list(APPEND SHADER_TARGETS
        lsi_rt_rgen_spv
        lsi_rt_miss_spv
        lsi_rt_rint_spv
        lsi_rt_rahit_spv
        lsi_rt_rchit_spv
)

list(APPEND SPIRV_OUTPUTS
        ${LSI_RGEN_SPV}
        ${LSI_MISS_SPV}
        ${LSI_RINT_SPV}
        ${LSI_RAHIT_SPV}
        ${LSI_RCHIT_SPV}
)

add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# =========================== LSI_VK Executable =============================
add_executable(polyover_vk_exec main.cc)

target_link_libraries(polyover_vk_exec PRIVATE
        rayjoin_lib_vk
)

add_dependencies(polyover_vk_exec ${SHADER_TARGETS})