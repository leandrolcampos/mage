# CMake module for configuring the LLVM toolchain used by Mage.
#
# This module validates MAGE_LLVM_ROOT and derives the LLVM toolchain paths used
# by the rest of the build.

include_guard(GLOBAL)

function(mage_validate_and_resolve_llvm_root out_var)
  if(NOT DEFINED MAGE_LLVM_ROOT OR MAGE_LLVM_ROOT STREQUAL "")
    message(FATAL_ERROR
      "MAGE_LLVM_ROOT must be set to the LLVM install prefix used by "
      "Mage, for example: -DMAGE_LLVM_ROOT=/path/to/llvm/install")
  endif()

  if(NOT IS_ABSOLUTE "${MAGE_LLVM_ROOT}")
    message(FATAL_ERROR
      "MAGE_LLVM_ROOT must be an absolute path, got '${MAGE_LLVM_ROOT}'")
  endif()

  if(NOT IS_DIRECTORY "${MAGE_LLVM_ROOT}")
    message(FATAL_ERROR
      "MAGE_LLVM_ROOT does not name an existing directory: "
      "'${MAGE_LLVM_ROOT}'")
  endif()

  file(REAL_PATH "${MAGE_LLVM_ROOT}" mage_llvm_root)
  set(${out_var} "${mage_llvm_root}" PARENT_SCOPE)
endfunction()

function(mage_get_cxx_compiler_from_llvm_root out_var)
  mage_validate_and_resolve_llvm_root(mage_llvm_root)

  set(mage_cxx_compiler "${mage_llvm_root}/bin/clang++")

  if(NOT EXISTS "${mage_cxx_compiler}")
    message(FATAL_ERROR
      "MAGE_LLVM_ROOT does not contain bin/clang++: '${mage_cxx_compiler}'")
  endif()

  file(REAL_PATH "${mage_cxx_compiler}" mage_cxx_compiler_real)
  set(${out_var} "${mage_cxx_compiler_real}" PARENT_SCOPE)
endfunction()

function(mage_set_cxx_compiler_from_llvm_root_if_unset)
  if(CMAKE_CXX_COMPILER_LOADED)
    message(FATAL_ERROR
      "mage_set_cxx_compiler_from_llvm_root_if_unset() must be called before "
      "project() or enable_language(CXX)")
  endif()

  if(DEFINED CMAKE_CXX_COMPILER AND NOT CMAKE_CXX_COMPILER STREQUAL "")
    return()
  endif()

  mage_get_cxx_compiler_from_llvm_root(mage_cxx_compiler)

  set(CMAKE_CXX_COMPILER "${mage_cxx_compiler}" CACHE FILEPATH
    "C++ compiler used by Mage" FORCE)
endfunction()

function(mage_validate_cxx_compiler_from_llvm_root)
  if(NOT CMAKE_CXX_COMPILER_LOADED)
    message(FATAL_ERROR
      "mage_validate_cxx_compiler_from_llvm_root() must be called after "
      "project() or enable_language(CXX)")
  endif()

  mage_get_cxx_compiler_from_llvm_root(mage_expected_cxx_compiler)

  file(REAL_PATH "${CMAKE_CXX_COMPILER}" mage_current_cxx_compiler)

  if(NOT mage_current_cxx_compiler STREQUAL mage_expected_cxx_compiler)
    message(FATAL_ERROR
      "CMAKE_CXX_COMPILER must match the C++ compiler derived from "
      "MAGE_LLVM_ROOT; expected '${mage_expected_cxx_compiler}', got "
      "'${mage_current_cxx_compiler}'; configure Mage with MAGE_LLVM_ROOT "
      "only or use a fresh build directory")
  endif()
endfunction()

function(mage_get_llvm_cmake_dir_from_root llvm_root out_var)
  if(EXISTS "${llvm_root}/lib/cmake/llvm/LLVMConfig.cmake")
    set(${out_var} "${llvm_root}/lib/cmake/llvm" PARENT_SCOPE)
    return()
  endif()

  if(EXISTS "${llvm_root}/lib64/cmake/llvm/LLVMConfig.cmake")
    set(${out_var} "${llvm_root}/lib64/cmake/llvm" PARENT_SCOPE)
    return()
  endif()

  message(FATAL_ERROR
    "MAGE_LLVM_ROOT='${MAGE_LLVM_ROOT}' does not contain LLVMConfig.cmake "
    "under lib/cmake/llvm or lib64/cmake/llvm")
endfunction()

function(mage_configure_llvm_toolchain)
  mage_validate_cxx_compiler_from_llvm_root()

  mage_validate_and_resolve_llvm_root(mage_llvm_root)
  mage_get_llvm_cmake_dir_from_root("${mage_llvm_root}" mage_llvm_cmake_dir)

  # Use the LLVM CMake package from MAGE_LLVM_ROOT only.
  set(LLVM_DIR "${mage_llvm_cmake_dir}")
  find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)

  set(MAGE_LLVM_CMAKE_DIR "${mage_llvm_cmake_dir}" CACHE INTERNAL
    "LLVM CMake package directory used by Mage" FORCE)
  set(MAGE_LLVM_VERSION "${LLVM_PACKAGE_VERSION}" CACHE INTERNAL
    "LLVM version used by Mage" FORCE)
  set(MAGE_LLVM_TOOLS_DIR "${LLVM_TOOLS_BINARY_DIR}" CACHE INTERNAL
    "LLVM tools directory reported by LLVMConfig.cmake" FORCE)
  set(MAGE_LLVM_LIBRARY_DIR "${LLVM_LIBRARY_DIR}" CACHE INTERNAL
    "LLVM library directory reported by LLVMConfig.cmake" FORCE)

  message(STATUS
    "Found LLVM: ${mage_llvm_root} (found version \"${MAGE_LLVM_VERSION}\")")
endfunction()

function(mage_configure_llvm_gpu_loader)
  if((NOT DEFINED MAGE_LLVM_TOOLS_DIR) OR (MAGE_LLVM_TOOLS_DIR STREQUAL ""))
    message(FATAL_ERROR
      "mage_configure_llvm_gpu_loader() requires "
      "mage_configure_llvm_toolchain() to be called first")
  endif()

  # Use llvm-gpu-loader from MAGE_LLVM_ROOT only.
  find_program(mage_llvm_gpu_loader
    NAMES llvm-gpu-loader
    PATHS "${MAGE_LLVM_TOOLS_DIR}"
    NO_DEFAULT_PATH)

  if(NOT mage_llvm_gpu_loader)
    message(FATAL_ERROR
      "llvm-gpu-loader was not found in '${MAGE_LLVM_TOOLS_DIR}'; make sure "
      "MAGE_LLVM_ROOT points to an LLVM installation with llvm-gpu-loader")
  endif()

  set(MAGE_LLVM_GPU_LOADER "${mage_llvm_gpu_loader}" CACHE INTERNAL
    "llvm-gpu-loader used by Mage" FORCE)

  unset(mage_llvm_gpu_loader CACHE)

  set(MAGE_LLVM_GPU_LOADER_ARGS "--blocks 1 --threads 1" CACHE INTERNAL
    "Arguments passed to llvm-gpu-loader when running GPU tests" FORCE)
endfunction()
