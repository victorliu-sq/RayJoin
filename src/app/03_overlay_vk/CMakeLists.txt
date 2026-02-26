# =========================== Shader

# Common flags you already use (keep scalar layout for safer structs)
set(SLANG_COMMON_FLAGS -profile sm_6_6 -target spirv -fvk-use-scalar-layout)

# Compile compute shader: scale_points_d2_i64.slang -> scale_points_d2_i64.spv
slang_compile_spirv(
        NAME scale_points
        SLANGC ${CMAKE_SOURCE_DIR}/deps/slang/bin/slangc
        SOURCE ${CMAKE_SOURCE_DIR}/src/vk/shader/scale_points_d2_i64.slang
        OUT_DIR ${SPV_OUTPUT_DIR}
        FLAGS ${SLANG_COMMON_FLAGS}
        ENTRIES
        main compute scale_points_d2_i64.spv
)

# =========================== LSI_VK Executable =======================================
add_executable(polyover_vk_exec main.cc)

target_link_libraries(polyover_vk_exec PRIVATE
        rayjoin_lib
)

target_compile_options(polyover_vk_exec PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
)

set_target_properties(polyover_vk_exec PROPERTIES
        CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
        CUDA_SEPARABLE_COMPILATION ON
)

add_dependencies(polyover_vk_exec optix_ptx)
# Make executable depend on the shader build
add_dependencies(polyover_vk_exec ${scale_points_SPV_TARGET})