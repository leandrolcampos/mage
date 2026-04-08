# CMake module for locating and validating the LLVM toolchain used by Mage.
#
# It treats the LLVM CMake package as the source of truth, derives a default
# LLVM_DIR from MAGE_LLVM_ROOT when possible, validates minimum tool versions,
# and discovers llvm-gpu-loader from the selected LLVM installation.

include_guard(GLOBAL)

set(MAGE_MINIMUM_LLVM_VERSION "23.0.0" CACHE INTERNAL
  "Minimum LLVM version required by Mage")

function(mage_extract_numeric_version out_var raw_version)
  string(REGEX MATCH "[0-9]+(\\.[0-9]+)(\\.[0-9]+)?"
    numeric_version "${raw_version}")

  if(numeric_version STREQUAL "")
    message(FATAL_ERROR
      "could not parse a version number from '${raw_version}'")
  endif()

  set(${out_var} "${numeric_version}" PARENT_SCOPE)
endfunction()

function(mage_get_program_version out_var program_path)
  if(NOT EXISTS "${program_path}")
    message(FATAL_ERROR
      "program does not exist: '${program_path}'")
  endif()

  execute_process(
    COMMAND "${program_path}" --version
    RESULT_VARIABLE cmd_result
    OUTPUT_VARIABLE cmd_output
    ERROR_VARIABLE cmd_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NOT cmd_result EQUAL 0)
    message(FATAL_ERROR
      "failed to execute '${program_path} --version'\n"
      "output: ${cmd_output}\n"
      "error: ${cmd_error}")
  endif()

  mage_extract_numeric_version(program_version "${cmd_output}")
  set(${out_var} "${program_version}" PARENT_SCOPE)
endfunction()

function(mage_try_set_llvm_dir_from_root llvm_root)
  if("${llvm_root}" STREQUAL "")
    return()
  endif()

  if(EXISTS "${llvm_root}/lib/cmake/llvm/LLVMConfig.cmake")
    set(LLVM_DIR "${llvm_root}/lib/cmake/llvm" CACHE PATH
      "Path to the LLVM CMake package directory used by Mage" FORCE)
    return()
  endif()

  if(EXISTS "${llvm_root}/lib64/cmake/llvm/LLVMConfig.cmake")
    set(LLVM_DIR "${llvm_root}/lib64/cmake/llvm" CACHE PATH
      "Path to the LLVM CMake package directory used by Mage" FORCE)
  endif()
endfunction()

function(mage_set_default_llvm_root)
  if(NOT MAGE_LLVM_ROOT STREQUAL "")
    return()
  endif()

  if(DEFINED ENV{LLVM_HOME} AND NOT "$ENV{LLVM_HOME}" STREQUAL "")
    set(mage_default_llvm_root "$ENV{LLVM_HOME}")
  else()
    get_filename_component(mage_clang_bin_dir
      "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(mage_default_llvm_root
      "${mage_clang_bin_dir}" DIRECTORY)
  endif()

  set(MAGE_LLVM_ROOT "${mage_default_llvm_root}" CACHE PATH
    "LLVM install prefix used by Mage" FORCE)
endfunction()

# Validates the selected Clang compiler, loads the LLVM CMake package, and
# resolves LLVM-hosted tools used by Mage.
function(mage_configure_llvm_toolchain)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
      "Mage must be built with clang; got '${CMAKE_CXX_COMPILER_ID}'")
  endif()

  mage_extract_numeric_version(mage_clang_version
    "${CMAKE_CXX_COMPILER_VERSION}")
  if(mage_clang_version VERSION_LESS MAGE_MINIMUM_LLVM_VERSION)
    message(FATAL_ERROR
      "Mage requires clang ${MAGE_MINIMUM_LLVM_VERSION} or newer; got "
      "${CMAKE_CXX_COMPILER_VERSION}")
  endif()

  set(mage_user_provided_llvm_root OFF)
  if(NOT MAGE_LLVM_ROOT STREQUAL "")
    set(mage_user_provided_llvm_root ON)
  endif()

  mage_set_default_llvm_root()

  if(LLVM_DIR STREQUAL "")
    mage_try_set_llvm_dir_from_root("${MAGE_LLVM_ROOT}")
  endif()

  if(LLVM_DIR STREQUAL "" AND mage_user_provided_llvm_root)
    message(FATAL_ERROR
      "MAGE_LLVM_ROOT='${MAGE_LLVM_ROOT}' does not contain LLVMConfig.cmake "
      "under lib/cmake/llvm or lib64/cmake/llvm")
  endif()

  find_package(LLVM REQUIRED CONFIG)

  mage_extract_numeric_version(mage_llvm_version
    "${LLVM_PACKAGE_VERSION}")
  if(mage_llvm_version VERSION_LESS MAGE_MINIMUM_LLVM_VERSION)
    message(FATAL_ERROR
      "Mage requires LLVM ${MAGE_MINIMUM_LLVM_VERSION} or newer; got "
      "${LLVM_PACKAGE_VERSION}")
  endif()

  set(MAGE_LLVM_VERSION "${mage_llvm_version}" CACHE INTERNAL
    "LLVM version used by Mage" FORCE)
  set(MAGE_LLVM_TOOLS_DIR "${LLVM_TOOLS_BINARY_DIR}" CACHE INTERNAL
    "LLVM tools directory used by Mage" FORCE)
  set(MAGE_LLVM_LIBRARY_DIR "${LLVM_LIBRARY_DIR}" CACHE INTERNAL
    "LLVM library directory used by Mage" FORCE)
  set(MAGE_LLVM_CMAKE_DIR "${LLVM_CMAKE_DIR}" CACHE INTERNAL
    "LLVM CMake package directory used by Mage" FORCE)

  if(MAGE_GPU_LOADER STREQUAL "")
    find_program(mage_default_gpu_loader
      NAMES llvm-gpu-loader
      HINTS "${LLVM_TOOLS_BINARY_DIR}")

    if(mage_default_gpu_loader)
      set(MAGE_GPU_LOADER "${mage_default_gpu_loader}" CACHE FILEPATH
        "Program used to run GPU unit tests. Defaults to llvm-gpu-loader"
        FORCE)
    endif()

    unset(mage_default_gpu_loader CACHE)
    unset(mage_default_gpu_loader)
  endif()

  if(MAGE_GPU_LOADER STREQUAL "" AND MAGE_GPU_TARGETS)
    message(STATUS
      "no GPU test loader was found automatically; GPU unit-test execution "
      "may require MAGE_GPU_LOADER to be set manually")
  endif()

  message(STATUS
    "Found LLVM ${LLVM_PACKAGE_VERSION} in '${LLVM_DIR}'")
endfunction()

# Applies Mage's global assertion policy.
function(mage_configure_assertions)
  if(MAGE_ENABLE_ASSERTIONS)
    add_compile_options(-UNDEBUG)
    return()
  endif()

  add_compile_definitions(NDEBUG)
endfunction()
