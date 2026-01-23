include_directories(${OptiX_INCLUDE})
include_directories(${PROJECT_SOURCE_DIR}/deps/Declib)
include_directories(${PROJECT_SOURCE_DIR}/deps/lbvh)
include_directories(${PROJECT_SOURCE_DIR}/src)

#set(GLOG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/include")

set(OPTIX_MODULE_EXTENSION ".ptx")
set(OPTIX_PROGRAM_TARGET "--ptx")

set(ENABLED_ARCHS "86")

set(SHADERS
    algo/rt_lsi_custom.cu
    algo/rt_pip_custom.cu
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

add_library(polyover_lib SHARED
        run_overlay.cu
        rt/rt_engine.cu
        flags.cu
        ${PROGRAM_MODULES}
)

target_link_libraries(polyover_lib PUBLIC
        glog::glog
        CUDA::cudart
        CUDA::cuda_driver
)
#add_dependencies(polyover_lib optix_ptx)

target_compile_options(polyover_lib PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>)
set_target_properties(polyover_lib PROPERTIES CUDA_ARCHITECTURES "${ENABLED_ARCHS}" CUDA_SEPARABLE_COMPILATION ON) # 70;

add_executable(polyover_exec
        overlay.cu
        )

target_link_libraries(polyover_exec PRIVATE
        polyover_lib
        )
target_compile_options(polyover_exec PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>)
set_target_properties(polyover_exec PROPERTIES CUDA_ARCHITECTURES "${ENABLED_ARCHS}" CUDA_SEPARABLE_COMPILATION ON) # 70;
add_dependencies(polyover_exec optix_ptx)

add_library(query_lib
        run_query.cu
        rt/rt_engine.cu
        flags.cu
)


target_link_libraries(query_lib PUBLIC
        CUDA::cudart
        CUDA::cuda_driver
        glog::glog
)
target_compile_options(query_lib PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>)
set_target_properties(query_lib PROPERTIES CUDA_ARCHITECTURES "${ENABLED_ARCHS}" CUDA_SEPARABLE_COMPILATION ON) # 70;

add_executable(query_exec
        query.cu
)

target_link_libraries(query_exec PRIVATE
        query_lib
)
target_compile_options(query_exec PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr>)
set_target_properties(query_exec PROPERTIES CUDA_ARCHITECTURES "${ENABLED_ARCHS}" CUDA_SEPARABLE_COMPILATION ON) # 70;
add_dependencies(query_exec optix_ptx)
