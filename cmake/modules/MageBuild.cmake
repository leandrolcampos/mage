# Helpers for configuring Mage host and GPU builds.

include_guard(GLOBAL)

include(CheckCXXCompilerFlag)
include(ExternalProject)
include(MageLLVM)

if(NOT DEFINED MAGE_INTERNAL_GPU_BUILD)
  set(MAGE_INTERNAL_GPU_BUILD OFF)
endif()

if(NOT DEFINED MAGE_INTERNAL_TARGET_TRIPLE)
  set(MAGE_INTERNAL_TARGET_TRIPLE "default")
endif()

function(mage_normalize_gpu_target_triples)
  set(normalized_gpu_target_triples)

  foreach(gpu_target_triple IN LISTS MAGE_GPU_TARGET_TRIPLES)
    string(STRIP "${gpu_target_triple}" gpu_target_triple)
    string(TOLOWER "${gpu_target_triple}" gpu_target_triple)

    if(gpu_target_triple STREQUAL "")
      continue()
    endif()

    list(APPEND normalized_gpu_target_triples "${gpu_target_triple}")
  endforeach()

  list(REMOVE_DUPLICATES normalized_gpu_target_triples)

  set(MAGE_GPU_TARGET_TRIPLES
    "${normalized_gpu_target_triples}"
    CACHE STRING
    "Semicolon-separated GPU target triples to build. May be empty"
    FORCE)
endfunction()

function(mage_validate_gpu_target_triples)
  set(supported_gpu_target_triples
    amdgcn-amd-amdhsa
    nvptx64-nvidia-cuda)

  list(JOIN supported_gpu_target_triples
    ", "
    supported_gpu_target_triples_str)

  foreach(gpu_target_triple IN LISTS MAGE_GPU_TARGET_TRIPLES)
    if(NOT gpu_target_triple IN_LIST supported_gpu_target_triples)
      message(FATAL_ERROR
        "unsupported GPU target triple '${gpu_target_triple}' in "
        "MAGE_GPU_TARGET_TRIPLES; expected one of: "
        "${supported_gpu_target_triples_str}")
    endif()
  endforeach()
endfunction()

# Checks whether the host toolchain can resolve a native GPU architecture.
function(_mage_check_native_gpu_arch_support out_var gpu_target_triple)
  set(old_try_compile_target_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
  set(old_required_flags "${CMAKE_REQUIRED_FLAGS}")

  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

  if(gpu_target_triple STREQUAL "amdgcn-amd-amdhsa")
    set(CMAKE_REQUIRED_FLAGS "--target=amdgcn-amd-amdhsa")
    check_cxx_compiler_flag("-mcpu=native"
                            MAGE_CHECK_AMDGPU_MCPU_NATIVE)
    set(${out_var} "${MAGE_CHECK_AMDGPU_MCPU_NATIVE}" PARENT_SCOPE)
  elseif(gpu_target_triple STREQUAL "nvptx64-nvidia-cuda")
    set(CMAKE_REQUIRED_FLAGS "--target=nvptx64-nvidia-cuda")
    check_cxx_compiler_flag("-march=native"
                            MAGE_CHECK_NVPTX_MARCH_NATIVE)
    set(${out_var} "${MAGE_CHECK_NVPTX_MARCH_NATIVE}" PARENT_SCOPE)
  else()
    message(FATAL_ERROR
      "unsupported GPU target triple in _mage_check_native_gpu_arch_support: "
      "${gpu_target_triple}")
  endif()

  set(CMAKE_REQUIRED_FLAGS "${old_required_flags}")
  set(CMAKE_TRY_COMPILE_TARGET_TYPE "${old_try_compile_target_type}")
endfunction()

# Resolves the context for the current build, including the target triple,
# backend classification, and optional GPU architecture. Resolved values are
# persisted in CACHE INTERNAL so subdirectories can observe them.
function(mage_resolve_current_build_context)
  set(target_triple "${MAGE_INTERNAL_TARGET_TRIPLE}")
  set(build_is_gpu OFF)
  set(build_is_amdgpu OFF)
  set(build_is_nvptx OFF)
  set(gpu_architecture "")

  if(target_triple STREQUAL "default")
    # Host build.
  elseif(target_triple STREQUAL "amdgcn-amd-amdhsa")
    set(build_is_gpu ON)
    set(build_is_amdgpu ON)
    _mage_check_native_gpu_arch_support(
      host_can_resolve_gpu_native_arch "amdgcn-amd-amdhsa")

    if(NOT MAGE_FORCE_AMDGPU_ARCHITECTURE STREQUAL "")
      set(gpu_architecture "${MAGE_FORCE_AMDGPU_ARCHITECTURE}")
    elseif(host_can_resolve_gpu_native_arch)
      set(gpu_architecture "native")
    else()
      message(FATAL_ERROR
        "cannot configure the AMDGPU build because no GPU architecture was "
        "detected or provided; set MAGE_FORCE_AMDGPU_ARCHITECTURE, for example "
        "gfx1101, or remove amdgcn-amd-amdhsa from MAGE_GPU_TARGET_TRIPLES")
    endif()
  elseif(target_triple STREQUAL "nvptx64-nvidia-cuda")
    set(build_is_gpu ON)
    set(build_is_nvptx ON)
    _mage_check_native_gpu_arch_support(
      host_can_resolve_gpu_native_arch "nvptx64-nvidia-cuda")

    if(NOT MAGE_FORCE_NVPTX_ARCHITECTURE STREQUAL "")
      set(gpu_architecture "${MAGE_FORCE_NVPTX_ARCHITECTURE}")
    elseif(host_can_resolve_gpu_native_arch)
      set(gpu_architecture "native")
    else()
      message(FATAL_ERROR
        "cannot configure the NVPTX build because no GPU architecture was "
        "detected or provided; set MAGE_FORCE_NVPTX_ARCHITECTURE, for example "
        "sm_120, or remove nvptx64-nvidia-cuda from MAGE_GPU_TARGET_TRIPLES")
    endif()
  else()
    message(FATAL_ERROR
      "unsupported target triple in mage_resolve_current_build_context: "
      "${target_triple}")
  endif()

  set(MAGE_TARGET_TRIPLE "${target_triple}" CACHE INTERNAL
    "Target triple for the current Mage build" FORCE)
  set(MAGE_BUILD_IS_GPU "${build_is_gpu}" CACHE INTERNAL
    "Whether the current Mage build targets a GPU" FORCE)
  set(MAGE_BUILD_IS_AMDGPU "${build_is_amdgpu}" CACHE INTERNAL
    "Whether the current Mage build targets AMDGPU" FORCE)
  set(MAGE_BUILD_IS_NVPTX "${build_is_nvptx}" CACHE INTERNAL
    "Whether the current Mage build targets NVPTX" FORCE)
  set(MAGE_GPU_ARCHITECTURE "${gpu_architecture}" CACHE INTERNAL
    "GPU architecture for the current Mage build" FORCE)
endfunction()

# Adds a GPU build rooted at build/<gpu_target_triple> and exposes convenience
# targets for building and testing it from the host build.
function(mage_add_gpu_build gpu_target_triple)
  set(gpu_build_binary_dir "${CMAKE_BINARY_DIR}/${gpu_target_triple}")
  set(gpu_build_config_target "configure-mage-${gpu_target_triple}")

  set(gpu_build_cmake_args
    "-DMAGE_INTERNAL_GPU_BUILD:BOOL=ON"
    "-DMAGE_INTERNAL_TARGET_TRIPLE:STRING=${gpu_target_triple}"
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DBUILD_TESTING:BOOL=${BUILD_TESTING}"
    "-DMAGE_LLVM_ROOT:PATH=${MAGE_LLVM_ROOT}"
    "-DMAGE_FORCE_ASSERTIONS:BOOL=${MAGE_FORCE_ASSERTIONS}")

  if(gpu_target_triple STREQUAL "amdgcn-amd-amdhsa" AND
     MAGE_FORCE_AMDGPU_ARCHITECTURE)
    set(gpu_architecture "${MAGE_FORCE_AMDGPU_ARCHITECTURE}")

    list(APPEND gpu_build_cmake_args
      "-DMAGE_FORCE_AMDGPU_ARCHITECTURE:STRING=${gpu_architecture}")
  elseif(gpu_target_triple STREQUAL "nvptx64-nvidia-cuda" AND
         MAGE_FORCE_NVPTX_ARCHITECTURE)
    set(gpu_architecture "${MAGE_FORCE_NVPTX_ARCHITECTURE}")

    list(APPEND gpu_build_cmake_args
      "-DMAGE_FORCE_NVPTX_ARCHITECTURE:STRING=${gpu_architecture}")
  endif()

  if(BUILD_TESTING)
    list(APPEND gpu_build_cmake_args
      "-DMAGE_GPU_TEST_PARALLELISM:STRING=${MAGE_GPU_TEST_PARALLELISM}")
  endif()

  ExternalProject_Add("${gpu_build_config_target}"
    PREFIX "${CMAKE_BINARY_DIR}/.gpu-builds/${gpu_target_triple}"
    SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    BINARY_DIR "${gpu_build_binary_dir}"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
      ${gpu_build_cmake_args}
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    EXCLUDE_FROM_ALL TRUE
    USES_TERMINAL_CONFIGURE TRUE)

  add_custom_target("mage-${gpu_target_triple}"
    COMMAND
      "${CMAKE_COMMAND}" --build "${gpu_build_binary_dir}" --target mage
    DEPENDS
      "${gpu_build_config_target}"
    USES_TERMINAL)

  add_dependencies(mage-all "mage-${gpu_target_triple}")

  if(BUILD_TESTING)
    add_custom_target("check-mage-${gpu_target_triple}"
      COMMAND
        "${CMAKE_COMMAND}" --build "${gpu_build_binary_dir}" --target check-mage
      DEPENDS
        "${gpu_build_config_target}"
      USES_TERMINAL)
  endif()
endfunction()
