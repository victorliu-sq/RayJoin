# ======================================================================== #
# Copyright 2018-2019 Ingo Wald                                            #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# ======================================================================== #

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

cmake_minimum_required(VERSION 3.10)

# ------------------------------------------------------------------
# IMPORTANT:
# Do NOT list CUDA in project() unconditionally.
# Otherwise CMake will require a CUDA compiler even when the user
# wants Vulkan-only (-DRAYJOIN_BUILD_OPTIX=OFF).
# ------------------------------------------------------------------
project(rayjoin LANGUAGES C CXX VERSION 1.0.1)

if (NOT WIN32)
  # Keep your current host-side C++ standard setting.
  set(CMAKE_CXX_FLAGS "--std=c++20")
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

if (WIN32 AND "${CMAKE_GENERATOR}" MATCHES "^(Visual Studio).*")
  # Base folder for generated PTX / OptiX artifacts.
  set(MODULE_TARGET_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$(ConfigurationName)")
  # Enable multi-processor build in Visual Studio.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
else ()
  set(MODULE_TARGET_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()

# ------------------------------------------------------------------
# Build options
#
# Design pattern requested:
#   - User controls OptiX/CUDA build explicitly
#   - No automatic GPU detection
# ------------------------------------------------------------------
option(RAYJOIN_BUILD_OPTIX "Build OptiX/CUDA backend" ON)
option(RAYJOIN_BUILD_VULKAN "Build Vulkan backend" ON)

message(STATUS "RAYJOIN_BUILD_OPTIX  = ${RAYJOIN_BUILD_OPTIX}")
message(STATUS "RAYJOIN_BUILD_VULKAN = ${RAYJOIN_BUILD_VULKAN}")

# Some useful macros
macro(ADD_TARGET_PROPERTIES _target _name)
  set(_properties)
  foreach (_prop ${ARGN})
    set(_properties "${_properties} ${_prop}")
  endforeach ()
  get_target_property(_old_properties ${_target} ${_name})
  if (NOT _old_properties)
    set(_old_properties)
  endif ()
  set_target_properties(${_target} PROPERTIES ${_name} "${_old_properties} ${_properties}")
endmacro()

macro(TARGET_INCLUDE_SYMBOL target symbol)
  if (WIN32)
    if (LOCAL_ARCH STREQUAL "amd64")
      add_target_properties(${target} LINK_FLAGS /include:${symbol})
    endif ()
  endif ()
  if (UNIX)
    add_target_properties(${target} LINK_FLAGS "-Wl,--undefined=${symbol}")
  endif ()
endmacro()

# ------------------------------------------------------------------
# CMake modules
# ------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/configure_build_type.cmake)
include(cmake/CheckReqPkg.cmake)
include(CompileSlang)

# ------------------------------------------------------------------
# Enable CUDA ONLY if the user asked for OptiX/CUDA build
# ------------------------------------------------------------------
if (RAYJOIN_BUILD_OPTIX)
  include(CheckLanguage)
  check_language(CUDA)

  if (NOT CMAKE_CUDA_COMPILER)
    message(FATAL_ERROR
            "RAYJOIN_BUILD_OPTIX=ON but no CUDA compiler was found. "
            "Either install CUDA or configure with -DRAYJOIN_BUILD_OPTIX=OFF")
  endif ()

  enable_language(CUDA)

  # OptiX/CUDA-related modules are only needed in this mode.
  include(cmake/FindOptiX.cmake)
  include(cmake/nvcuda_compile_module.cmake)

  # CUDAToolkit is required only for OptiX/CUDA mode.
  find_package(CUDAToolkit REQUIRED)

  if (NOT WIN32)
    set(CUDA_PROPAGATE_HOST_FLAGS ON CACHE STRING
            "Propagate C/CXX_FLAGS and friends to the host compiler via -Xcompile")
  endif ()
endif ()

# ============================================================================================
# Where to find dependencies.
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/deps")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/deps/vulkansdk/x86_64")

find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(GTest REQUIRED)

# Vulkan dependencies are only required if Vulkan backend is enabled.
if (RAYJOIN_BUILD_VULKAN)
  find_package(Vulkan REQUIRED)
  find_package(VulkanMemoryAllocator REQUIRED)
endif ()

check_required_package(gflags)
check_required_package(glog)
check_required_package(GTest)

if (RAYJOIN_BUILD_VULKAN)
  check_required_package(Vulkan)
  check_required_package(VulkanMemoryAllocator)
endif ()

# ============================================================================================

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
set(CMAKE_BUILD_RPATH ${INSTALL_PREFIX_LIBRARY})
set(CMAKE_INSTALL_RPATH ${INSTALL_PREFIX_LIBRARY})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ------------------------------------------------------------------
# Final build rules
# ------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(test)