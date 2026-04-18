# =========================== PIP Executable =========================================
if (NOT RAYJOIN_BUILD_OPTIX)
  return()
endif ()

add_executable(query_exec main.cc
)

target_link_libraries(query_exec PRIVATE
        rayjoin_lib
)

target_compile_options(query_exec PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr>
)

set_target_properties(query_exec PROPERTIES
        CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
        CUDA_SEPARABLE_COMPILATION ON
)

add_dependencies(query_exec optix_ptx)