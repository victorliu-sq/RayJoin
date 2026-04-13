# ======================================================================== #
# Copyright 2018-2019 Ingo Wald                                            #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
# ======================================================================== #

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

cmake_minimum_required(VERSION 3.10)

project(rayjoin LANGUAGES CUDA C CXX VERSION 1.0.1)

if (NOT WIN32)
  # visual studio doesn't like these (not need them):
  set(CMAKE_CXX_FLAGS "--std=c++20")
  set(CUDA_PROPAGATE_HOST_FLAGS ON CACHE STRING "Propagate C/CXX_FLAGS and friends to the host compiler via -Xcompile")
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

if (WIN32 AND "${CMAKE_GENERATOR}" MATCHES "^(Visual Studio).*")
  # Set the base folder where the per-project "core" folders with the *.ptx or *.optixir files get created.
  set(MODULE_TARGET_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$(ConfigurationName)")
  # Enable multi-processor build on all Visual Studio versions.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
else ()
  # DAR This should be independent of ${CMAKE_BUILD_TYPE} because that single-configuration generator will not create subfolders, will it?
  # Otherwise add something with if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(MODULE_TARGET_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()

# Some useful macros
macro(ADD_TARGET_PROPERTIES _target _name)
  set(_properties)
  foreach (_prop ${ARGN})
    set(_properties "${_properties} ${_prop}")
  endforeach (_prop)
  get_target_property(_old_properties ${_target} ${_name})
  if (NOT _old_properties)
    # In case it's NOTFOUND
    set(_old_properties)
  endif (NOT _old_properties)
  set_target_properties(${_target} PROPERTIES ${_name} "${_old_properties} ${_properties}")
endmacro(ADD_TARGET_PROPERTIES)

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

# third-party
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/configure_build_type.cmake)
include(cmake/FindOptiX.cmake)
include(cmake/nvcuda_compile_module.cmake)
# Slang Compiler Custom Commands
#include(cmake/CompileSlang.cmake)
include(CompileSlang)

#find_package(CUDA 11.8 REQUIRED)
find_package(CUDAToolkit REQUIRED)

# ============================================================================================
# Where to find these depepdencies.
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/deps")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/deps/vulkansdk/x86_64")

include(cmake/CheckReqPkg.cmake)

find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(GTest REQUIRED) # Notice the GT
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

check_required_package(gflags)
check_required_package(glog)
check_required_package(GTest)
check_required_package(Vulkan)
check_required_package(VulkanMemoryAllocator)

# ============================================================================================

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")  # this line of code allows cmake to search library path
set(CMAKE_BUILD_RPATH ${INSTALL_PREFIX_LIBRARY})
set(CMAKE_INSTALL_RPATH ${INSTALL_PREFIX_LIBRARY})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
# ------------------------------------------------------------------
# and final build rules for the project
# ------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(test)