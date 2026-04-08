cmake_minimum_required(VERSION 3.20)
project(Mage LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

set(mage_enable_assertions_default OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(mage_enable_assertions_default ON)
endif()

option(MAGE_ENABLE_ASSERTIONS
  "Enable code assertions. Defaults to ON when CMAKE_BUILD_TYPE is Debug"
  ${mage_enable_assertions_default})

set(MAGE_LLVM_ROOT "" CACHE PATH
  "LLVM install prefix used by Mage")
set(LLVM_DIR "" CACHE PATH
  "Path to the LLVM CMake package directory used by Mage")

set(MAGE_GPU_TARGETS
  "amdgcn-amd-amdhsa;nvptx64-nvidia-cuda"
  CACHE STRING
  "Semicolon-separated GPU targets to build. May be empty")

set(MAGE_FORCE_AMDGPU_ARCH "" CACHE STRING
  "Force building of AMDGPU device code with the given architecture")
set(MAGE_FORCE_NVPTX_ARCH "" CACHE STRING
  "Force building of NVPTX device code with the given architecture")

set(MAGE_GPU_LOADER "" CACHE FILEPATH
  "Program used to run GPU unit tests. Defaults to llvm-gpu-loader")
set(MAGE_GPU_LOADER_ARGS "--blocks 1 --threads 1" CACHE STRING
  "Arguments appended to MAGE_GPU_LOADER when running GPU tests")
set(MAGE_GPU_TEST_JOBS "1" CACHE STRING
  "Maximum number of GPU unit tests to run in parallel")

include(MageLLVM)
include(MageBuild)

mage_configure_llvm_toolchain()
mage_configure_assertions()
mage_configure_build()
