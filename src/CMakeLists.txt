# =========================== PTX Shaders ===================================
# add include directories for ptx shaders
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

# =========================== RayJoin-CUDA Library  ===================================
add_library(rayjoin_lib SHARED
        core/run_overlay.cu
        core/run_query.cu
        rt/rt_engine.cu
        flag/flags.cc
        ${PROGRAM_MODULES}
)

target_link_libraries(rayjoin_lib PUBLIC
        CUDA::cudart
        CUDA::cuda_driver
        glog::glog
)

# Put SPIR-V under: <build>/shaders/
#get_filename_component(APP_DIR_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
#set(SPV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders/${APP_DIR_NAME}")
#set(SPV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
#target_compile_definitions(rayjoin_lib PRIVATE
#        SHADER_DIR="${SPV_OUTPUT_DIR}")

target_include_directories(rayjoin_lib PUBLIC
        #       dependencies
        ${OptiX_INCLUDE}
        ${PROJECT_SOURCE_DIR}/deps/Declib
        ${PROJECT_SOURCE_DIR}/deps/lbvh
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_options(rayjoin_lib PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
)

set_target_properties(rayjoin_lib PROPERTIES
        CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
        CUDA_SEPARABLE_COMPILATION ON
)

add_dependencies(rayjoin_lib optix_ptx)

# =========================== Executables ===================================
add_subdirectory(app/01_overlay)
add_subdirectory(app/02_query)

# =========================== RayJoin-VK ===================================
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Build all libs with -fPIC" FORCE)

# Nvpro_core2
#find_package(NvproCore2 REQUIRED)
#if (NOT NvproCore2_FOUND)
#    message(FATAL_ERROR "NvproCore2 not found, please install the NvproCore2 library")
#else ()
#    message("NvproCore2 is found. with include directory as ${NvproCore2_INCLUDE_DIRS}")
#endif ()

add_library(rayjoin_lib_vk SHARED
#        core/run_overlay.cu
#        core/run_query.cu
#        rt/rt_engine.cu
        flag/flags.cc
#        ${PROGRAM_MODULES}
#       VK
        vk/core/run_overlay.cc
        vk/core/vk_global_context.cc
        vk/rt/rt_engine.cc
        vk/vma_iimpl.cc
)

target_link_libraries(rayjoin_lib_vk PUBLIC
#        CUDA::cudart
#        CUDA::cuda_driver
        glog::glog
        #       Vulkan
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        #       Nvpro2
#        nvpro2::nvapp
#        nvpro2::nvutils
#        nvpro2::nvvk
)

# Put SPIR-V under: <build>/shaders/
#get_filename_component(APP_DIR_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
#set(SPV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders/${APP_DIR_NAME}")
set(SPV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
target_compile_definitions(rayjoin_lib_vk PRIVATE
        SHADER_DIR="${SPV_OUTPUT_DIR}")

#target_include_directories(rayjoin_lib_vk PUBLIC
        #       dependencies
#        ${OptiX_INCLUDE}
#        ${PROJECT_SOURCE_DIR}/deps/Declib
#        ${PROJECT_SOURCE_DIR}/deps/lbvh
#        ${PROJECT_SOURCE_DIR}/src
#)

#target_compile_options(rayjoin_lib_vk PRIVATE
#        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr --prec-div=true>
#)

#set_target_properties(rayjoin_lib_vk PROPERTIES
#        CUDA_ARCHITECTURES "${ENABLED_ARCHS}"
#        CUDA_SEPARABLE_COMPILATION ON
#)

add_dependencies(rayjoin_lib_vk optix_ptx)

# =========================== Executables ===================================
add_subdirectory(app/03_overlay_vk)