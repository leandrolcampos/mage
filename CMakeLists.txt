cmake_minimum_required(VERSION 3.20)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

set(MAGE_LLVM_ROOT "" CACHE PATH "LLVM install prefix used by Mage")

include(MageLLVM)

# CMAKE_CXX_COMPILER must be set before project() enables CXX.
mage_set_cxx_compiler_from_llvm_root_if_unset()

project(Mage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(MAGE_FORCE_ASSERTIONS "Force assertions in non-Debug builds" OFF)

if(MAGE_FORCE_ASSERTIONS)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" cmake_build_type_uppercase)
  if(NOT cmake_build_type_uppercase STREQUAL "DEBUG")
    add_compile_options(-UNDEBUG)
  endif()
endif()

set(MAGE_GPU_TARGET_TRIPLES
  "amdgcn-amd-amdhsa;nvptx64-nvidia-cuda"
  CACHE STRING
  "Semicolon-separated GPU target triples to build. May be empty")

set(MAGE_FORCE_AMDGPU_ARCH "" CACHE STRING
  "Force building of AMDGPU device code with the given architecture")
set(MAGE_FORCE_NVPTX_ARCH "" CACHE STRING
  "Force building of NVPTX device code with the given architecture")

set(MAGE_GPU_TEST_PARALLELISM "1" CACHE STRING
  "Maximum number of GPU unit tests to run in parallel")

include(MageBuild)

mage_configure_llvm_toolchain()
mage_configure_build()
