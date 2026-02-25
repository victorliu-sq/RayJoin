# =========================== LSI_VK Executable =======================================
add_executable(polyover_vk_exec main.cc)

target_link_libraries(polyover_vk_exec PRIVATE
        rayjoin_lib
        GPUOpen::VulkanMemoryAllocator
)

target_compile_options(polyover_vk_exec PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
)

set_target_properties(polyover_vk_exec PROPERTIES
        CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
        CUDA_SEPARABLE_COMPILATION ON
)

add_dependencies(polyover_vk_exec optix_ptx)