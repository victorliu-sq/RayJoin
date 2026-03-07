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
set(SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc)
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src/vk/shader)

file(GLOB SLANG_SHADERS
        ${SHADER_DIR}/*.slang
)

set(SHADER_TARGETS "")
set(SPIRV_OUTPUTS "")

foreach (SHADER ${SLANG_SHADERS})

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
          COMMENT "Compiling ${SHADER_NAME}.slang"
          VERBATIM
  )

  # create a target per shader
  set(TARGET_NAME ${SHADER_NAME}_spv)

  add_custom_target(${TARGET_NAME}
          DEPENDS ${SPIRV}
  )

  list(APPEND SHADER_TARGETS ${TARGET_NAME})
  list(APPEND SPIRV_OUTPUTS ${SPIRV})

endforeach ()

# optional global target
add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# =========================== LSI_VK Executable =======================================
add_executable(polyover_vk_exec main.cc)

target_link_libraries(polyover_vk_exec PRIVATE
        rayjoin_lib_vk
)

#target_compile_options(polyover_vk_exec PRIVATE
#        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
#)

#set_target_properties(polyover_vk_exec PROPERTIES
#        CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
#        CUDA_SEPARABLE_COMPILATION ON
#)

#add_dependencies(polyover_vk_exec optix_ptx)

# Make executable depend on the shader build
#add_dependencies(polyover_vk_exec ${scale_points_SPV_TARGET})
#add_dependencies(polyover_vk_exec ${edge_init_SPV_TARGET})
#add_dependencies(polyover_vk_exec ${edge_init_dev_addr_SPV_TARGET})
add_dependencies(polyover_vk_exec ${SHADER_TARGETS})