# =========================== Shader =======================================
set(SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc)
set(SHADER_DIR_NS ${CMAKE_SOURCE_DIR}/src/vk/shader/kernel_ns)


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
          -I ${SHADER_DIR_NS}/module
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
