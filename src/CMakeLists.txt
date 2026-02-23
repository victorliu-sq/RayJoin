include_directories(${OptiX_INCLUDE})
include_directories(${PROJECT_SOURCE_DIR}/deps/Declib)
include_directories(${PROJECT_SOURCE_DIR}/deps/lbvh)
include_directories(${PROJECT_SOURCE_DIR}/src)

set(OPTIX_MODULE_EXTENSION ".ptx")
set(OPTIX_PROGRAM_TARGET "--ptx")
set(ENABLED_ARCHS "86")

set(SHADERS
    shader/rt_lsi_custom.cu
    shader/rt_pip_custom.cu
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

message("PROGRAM_MODULES: ${PROGRAM_MODULES}")

add_custom_target(optix_ptx ALL
    DEPENDS ${PROGRAM_MODULES}
)

# ---- merged library ----
add_library(rayjoin_lib SHARED
    run_overlay.cu
    run_query.cu
    rt/rt_engine.cu
    flags.cu
    ${PROGRAM_MODULES}
)

target_link_libraries(rayjoin_lib PUBLIC
    CUDA::cudart
    CUDA::cuda_driver
    glog::glog
)

target_compile_options(rayjoin_lib PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
)

set_target_properties(rayjoin_lib PROPERTIES
    CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
    CUDA_SEPARABLE_COMPILATION ON
)

add_dependencies(rayjoin_lib optix_ptx)

# ---- executables ----
# =========================== LSI =====================================================
add_executable(polyover_exec
    overlay.cu
)

target_link_libraries(polyover_exec PRIVATE
    rayjoin_lib
)

target_compile_options(polyover_exec PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
)

set_target_properties(polyover_exec PROPERTIES
    CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
    CUDA_SEPARABLE_COMPILATION ON
)

add_dependencies(polyover_exec optix_ptx)

# =========================== PIP =====================================================
add_executable(query_exec
    query.cu
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