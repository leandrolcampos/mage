# Configures the LLVM toolchain used by Mage.

include_guard(GLOBAL)

function(_mage_validate_and_resolve_llvm_root out_var)
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

  file(REAL_PATH "${MAGE_LLVM_ROOT}" llvm_root)
  set(${out_var} "${llvm_root}" PARENT_SCOPE)
endfunction()

function(_mage_get_cxx_compiler_from_llvm_root out_var)
  _mage_validate_and_resolve_llvm_root(llvm_root)

  set(cxx_compiler "${llvm_root}/bin/clang++")

  if(NOT EXISTS "${cxx_compiler}")
    message(FATAL_ERROR
      "MAGE_LLVM_ROOT does not contain bin/clang++: '${cxx_compiler}'")
  endif()

  set(${out_var} "${cxx_compiler}" PARENT_SCOPE)
endfunction()

# Selects MAGE_LLVM_ROOT/bin/clang++ as the C++ compiler before CXX is enabled.
function(mage_set_cxx_compiler_from_llvm_root_if_unset)
  if(CMAKE_CXX_COMPILER_LOADED)
    message(FATAL_ERROR
      "mage_set_cxx_compiler_from_llvm_root_if_unset() must be called before "
      "project() or enable_language(CXX)")
  endif()

  if(DEFINED CMAKE_CXX_COMPILER AND NOT CMAKE_CXX_COMPILER STREQUAL "")
    return()
  endif()

  _mage_get_cxx_compiler_from_llvm_root(cxx_compiler)

  set(CMAKE_CXX_COMPILER "${cxx_compiler}" CACHE FILEPATH
    "C++ compiler used by Mage" FORCE)
endfunction()

function(_mage_validate_cxx_compiler_from_llvm_root)
  if(NOT CMAKE_CXX_COMPILER_LOADED)
    message(FATAL_ERROR
      "_mage_validate_cxx_compiler_from_llvm_root() must be called after "
      "project() or enable_language(CXX)")
  endif()

  _mage_get_cxx_compiler_from_llvm_root(expected_cxx_compiler)

  file(REAL_PATH "${expected_cxx_compiler}" expected_cxx_compiler_real)
  file(REAL_PATH "${CMAKE_CXX_COMPILER}" current_cxx_compiler_real)

  if(NOT current_cxx_compiler_real STREQUAL expected_cxx_compiler_real)
    message(FATAL_ERROR
      "CMAKE_CXX_COMPILER must match the C++ compiler derived from "
      "MAGE_LLVM_ROOT; expected '${expected_cxx_compiler_real}', got "
      "'${current_cxx_compiler_real}'; configure Mage with MAGE_LLVM_ROOT "
      "only or use a fresh build directory")
  endif()
endfunction()

function(_mage_get_llvm_cmake_dir_from_root out_var llvm_root)
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

function(_mage_configure_llvm_components llvm_include_dirs llvm_definitions)
  if(llvm_include_dirs STREQUAL "")
    message(FATAL_ERROR
      "_mage_configure_llvm_components() requires "
      "LLVM include directories to be set")
  endif()

  if(llvm_definitions STREQUAL "")
    message(FATAL_ERROR
      "_mage_configure_llvm_components() requires "
      "LLVM compile definitions to be set")
  endif()

  add_library(MageLLVMCommon INTERFACE)

  target_include_directories(MageLLVMCommon SYSTEM INTERFACE
    ${llvm_include_dirs})

  separate_arguments(
    llvm_compile_definitions NATIVE_COMMAND "${llvm_definitions}")

  target_compile_options(MageLLVMCommon INTERFACE
    ${llvm_compile_definitions})

  add_library(MageLLVMSupport INTERFACE)
  add_library(Mage::LLVMSupport ALIAS MageLLVMSupport)

  target_link_libraries(MageLLVMSupport INTERFACE
    MageLLVMCommon
    LLVMSupport)
endfunction()

# Configures LLVM and the support targets used by Mage.
function(mage_configure_llvm_toolchain)
  _mage_validate_cxx_compiler_from_llvm_root()

  _mage_validate_and_resolve_llvm_root(llvm_root)
  _mage_get_llvm_cmake_dir_from_root(llvm_cmake_dir "${llvm_root}")

  # Use the LLVM CMake package from MAGE_LLVM_ROOT only.
  set(LLVM_DIR "${llvm_cmake_dir}")
  find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)

  set(MAGE_LLVM_LIBRARY_DIR "${LLVM_LIBRARY_DIR}" CACHE INTERNAL
    "LLVM library directory reported by LLVMConfig.cmake" FORCE)
  set(MAGE_LLVM_TOOLS_DIR "${LLVM_TOOLS_BINARY_DIR}" CACHE INTERNAL
    "LLVM tools directory reported by LLVMConfig.cmake" FORCE)

  message(STATUS
    "Found LLVM: ${llvm_root} (found version \"${LLVM_PACKAGE_VERSION}\")")

  _mage_configure_llvm_components(
    "${LLVM_INCLUDE_DIRS}"
    "${LLVM_DEFINITIONS}")
endfunction()

# Configures the LLVM libc target used by Mage targets in the current build.
function(mage_configure_llvm_libc)
  if(TARGET MageLLVMLibC)
    return()
  endif()

  if(NOT DEFINED MAGE_BUILD_KIND OR MAGE_BUILD_KIND STREQUAL "")
    message(FATAL_ERROR
      "mage_configure_llvm_libc() requires MAGE_BUILD_KIND to be set")
  endif()

  add_library(MageLLVMLibC INTERFACE)
  add_library(Mage::LLVMLibC ALIAS MageLLVMLibC)

  if(MAGE_BUILD_KIND STREQUAL "GPU")
    target_link_options(MageLLVMLibC INTERFACE
      -stdlib)
    return()
  endif()

  if((NOT DEFINED MAGE_LLVM_LIBRARY_DIR) OR
     (MAGE_LLVM_LIBRARY_DIR STREQUAL ""))
    message(FATAL_ERROR
      "mage_configure_llvm_libc() requires "
      "MAGE_LLVM_LIBRARY_DIR to be set")
  endif()

  if((NOT DEFINED MAGE_TARGET_TRIPLE) OR
     (MAGE_TARGET_TRIPLE STREQUAL ""))
    message(FATAL_ERROR
      "mage_configure_llvm_libc() requires MAGE_TARGET_TRIPLE to be set")
  endif()

  set(target_libc_dir
    "${MAGE_LLVM_LIBRARY_DIR}/${MAGE_TARGET_TRIPLE}")

  # Use libllvmlibc.a from MAGE_LLVM_ROOT only.
  find_library(llvm_libc
    NAMES libllvmlibc.a
    PATHS "${target_libc_dir}"
    NO_DEFAULT_PATH)

  if(NOT llvm_libc)
    message(FATAL_ERROR
      "libllvmlibc.a for target '${MAGE_TARGET_TRIPLE}' was not found in "
      "'${target_libc_dir}'; make sure MAGE_LLVM_ROOT points to an LLVM "
      "installation with LLVM libc for this target")
  endif()

  target_link_libraries(MageLLVMLibC INTERFACE
    "${llvm_libc}")

  unset(llvm_libc CACHE)
endfunction()

# Finds llvm-gpu-loader and records the command used to run GPU tests.
function(mage_configure_llvm_gpu_loader)
  if((NOT DEFINED MAGE_LLVM_TOOLS_DIR) OR (MAGE_LLVM_TOOLS_DIR STREQUAL ""))
    message(FATAL_ERROR
      "mage_configure_llvm_gpu_loader() requires "
      "MAGE_LLVM_TOOLS_DIR to be set")
  endif()

  # Use llvm-gpu-loader from MAGE_LLVM_ROOT only.
  find_program(llvm_gpu_loader
    NAMES llvm-gpu-loader
    PATHS "${MAGE_LLVM_TOOLS_DIR}"
    NO_DEFAULT_PATH)

  if(NOT llvm_gpu_loader)
    message(FATAL_ERROR
      "llvm-gpu-loader was not found in '${MAGE_LLVM_TOOLS_DIR}'; make sure "
      "MAGE_LLVM_ROOT points to an LLVM installation with llvm-gpu-loader")
  endif()

  set(MAGE_LLVM_GPU_LOADER "${llvm_gpu_loader}" CACHE INTERNAL
    "llvm-gpu-loader used by Mage" FORCE)

  unset(llvm_gpu_loader CACHE)

  set(MAGE_LLVM_GPU_LOADER_ARGS "--blocks 1 --threads 1" CACHE INTERNAL
    "Arguments passed to llvm-gpu-loader when running GPU tests" FORCE)
endfunction()
