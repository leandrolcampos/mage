# CMake module for configuring host build and optional GPU sub-builds.
#
# It resolves per-target build context, decides whether GPU unit tests
# are buildable on the current host, and creates GPU leaf builds using
# ExternalProject.

include_guard(GLOBAL)

include(CheckCXXCompilerFlag)
include(ExternalProject)
include(MageRules)

set(MAGE_INTERNAL_LEAF_BUILD OFF CACHE INTERNAL
  "Whether this configure is a leaf build spawned by the Mage superbuild")
set(MAGE_INTERNAL_TARGET_TRIPLE "default" CACHE INTERNAL
  "Target triple for a Mage leaf build")

function(mage_validate_gpu_targets)
  set(seen_gpu_targets)

  foreach(gpu_target IN LISTS MAGE_GPU_TARGETS)
    if(gpu_target STREQUAL "")
      continue()
    endif()

    if(NOT gpu_target STREQUAL "amdgcn-amd-amdhsa" AND
       NOT gpu_target STREQUAL "nvptx64-nvidia-cuda")
      message(FATAL_ERROR
        "unsupported GPU target '${gpu_target}' in MAGE_GPU_TARGETS")
    endif()

    if(gpu_target IN_LIST seen_gpu_targets)
      message(FATAL_ERROR
        "duplicate GPU target '${gpu_target}' in MAGE_GPU_TARGETS")
    endif()

    list(APPEND seen_gpu_targets "${gpu_target}")
  endforeach()
endfunction()

# Checks whether the current host toolchain can resolve a native architecture
# for the given GPU target.
function(mage_check_native_gpu_arch_support gpu_target out_var)
  set(old_try_compile_target_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
  set(old_required_flags "${CMAKE_REQUIRED_FLAGS}")

  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

  if(gpu_target STREQUAL "amdgcn-amd-amdhsa")
    set(CMAKE_REQUIRED_FLAGS "--target=amdgcn-amd-amdhsa")
    check_cxx_compiler_flag("-mcpu=native" HOST_CAN_RESOLVE_GPU_NATIVE_ARCH)
  elseif(gpu_target STREQUAL "nvptx64-nvidia-cuda")
    set(CMAKE_REQUIRED_FLAGS "--target=nvptx64-nvidia-cuda")
    check_cxx_compiler_flag("-march=native" HOST_CAN_RESOLVE_GPU_NATIVE_ARCH)
  else()
    message(FATAL_ERROR
      "unsupported GPU target in mage_check_native_gpu_arch_support: "
      "${gpu_target}")
  endif()

  set(CMAKE_REQUIRED_FLAGS "${old_required_flags}")
  set(CMAKE_TRY_COMPILE_TARGET_TYPE "${old_try_compile_target_type}")

  set(${out_var} "${HOST_CAN_RESOLVE_GPU_NATIVE_ARCH}" PARENT_SCOPE)
endfunction()

# Resolves the build context for the current leaf build, including the target
# triple, backend classification, optional GPU architecture, and test command.
# The resolved values are persisted in CACHE INTERNAL so subdirectories in the
# leaf build can observe them reliably.
function(mage_resolve_leaf_context)
  set(mage_target_triple "${MAGE_INTERNAL_TARGET_TRIPLE}")
  set(mage_target_is_gpu OFF)
  set(mage_target_is_amdgpu OFF)
  set(mage_target_is_nvptx OFF)
  set(mage_gpu_architecture "")

  if(mage_target_triple STREQUAL "default")
    # Host build.
  elseif(mage_target_triple STREQUAL "amdgcn-amd-amdhsa")
    set(mage_target_is_gpu ON)
    set(mage_target_is_amdgpu ON)
    mage_check_native_gpu_arch_support(
      "amdgcn-amd-amdhsa" host_can_resolve_gpu_native_arch)

    if(NOT MAGE_FORCE_AMDGPU_ARCH STREQUAL "")
      set(mage_gpu_architecture "${MAGE_FORCE_AMDGPU_ARCH}")
    elseif(host_can_resolve_gpu_native_arch)
      set(mage_gpu_architecture "native")
    endif()
  elseif(mage_target_triple STREQUAL "nvptx64-nvidia-cuda")
    set(mage_target_is_gpu ON)
    set(mage_target_is_nvptx ON)
    mage_check_native_gpu_arch_support(
      "nvptx64-nvidia-cuda" host_can_resolve_gpu_native_arch)

    if(NOT MAGE_FORCE_NVPTX_ARCH STREQUAL "")
      set(mage_gpu_architecture "${MAGE_FORCE_NVPTX_ARCH}")
    elseif(host_can_resolve_gpu_native_arch)
      set(mage_gpu_architecture "native")
    endif()
  else()
    message(FATAL_ERROR
      "unsupported target triple in mage_resolve_leaf_context: "
      "${mage_target_triple}")
  endif()

  set(MAGE_TARGET_TRIPLE "${mage_target_triple}" CACHE INTERNAL
    "Target triple for the current Mage build" FORCE)
  set(MAGE_TARGET_IS_GPU "${mage_target_is_gpu}" CACHE INTERNAL
    "Whether the current Mage build targets a GPU" FORCE)
  set(MAGE_TARGET_IS_AMDGPU "${mage_target_is_amdgpu}" CACHE INTERNAL
    "Whether the current Mage build targets AMDGPU" FORCE)
  set(MAGE_TARGET_IS_NVPTX "${mage_target_is_nvptx}" CACHE INTERNAL
    "Whether the current Mage build targets NVPTX" FORCE)
  set(MAGE_GPU_ARCHITECTURE "${mage_gpu_architecture}" CACHE INTERNAL
    "GPU architecture for the current Mage build" FORCE)
endfunction()

function(mage_should_build_unittests out_var)
  if(NOT BUILD_TESTING)
    set(${out_var} OFF PARENT_SCOPE)
    return()
  endif()

  if(NOT MAGE_TARGET_IS_GPU)
    set(${out_var} ON PARENT_SCOPE)
    return()
  endif()

  if(NOT MAGE_GPU_ARCHITECTURE STREQUAL "")
    set(${out_var} ON PARENT_SCOPE)
    return()
  endif()

  if(MAGE_TARGET_IS_AMDGPU)
    message(STATUS
      "skipping unit tests for amdgcn-amd-amdhsa because no AMDGPU "
      "architecture was detected or provided")
    set(${out_var} OFF PARENT_SCOPE)
    return()
  endif()

  if(MAGE_TARGET_IS_NVPTX)
    message(STATUS
      "skipping unit tests for nvptx64-nvidia-cuda because no NVPTX "
      "architecture was detected or provided")
    set(${out_var} OFF PARENT_SCOPE)
    return()
  endif()

  message(FATAL_ERROR
    "unsupported target triple in mage_should_build_unittests: "
    "${MAGE_TARGET_TRIPLE}")
endfunction()

function(mage_get_registered_library_targets out_archive_var out_bitcode_var)
  get_property(mage_archive_targets GLOBAL PROPERTY MAGE_ARCHIVE_TARGETS)
  get_property(mage_bitcode_targets GLOBAL PROPERTY MAGE_BITCODE_TARGETS)

  if(NOT mage_archive_targets)
    set(mage_archive_targets)
  endif()

  if(NOT mage_bitcode_targets)
    set(mage_bitcode_targets)
  endif()

  list(REMOVE_DUPLICATES mage_archive_targets)
  list(REMOVE_DUPLICATES mage_bitcode_targets)

  set(${out_archive_var} "${mage_archive_targets}" PARENT_SCOPE)
  set(${out_bitcode_var} "${mage_bitcode_targets}" PARENT_SCOPE)
 endfunction()

function(mage_configure_leaf_build)
  mage_resolve_leaf_context()

  add_subdirectory(lib)

  mage_get_registered_library_targets(
    mage_leaf_archive_targets
    mage_leaf_bitcode_targets)

  add_custom_target(mage)
  if(mage_leaf_archive_targets)
    add_dependencies(mage ${mage_leaf_archive_targets})
  endif()
  if(mage_leaf_bitcode_targets)
    add_dependencies(mage ${mage_leaf_bitcode_targets})
   endif()

  mage_should_build_unittests(mage_build_unittests)
  if(mage_build_unittests)
    add_subdirectory(unittests)

    set(mage_ctest_args
      --output-on-failure
      --test-dir "${CMAKE_BINARY_DIR}")

    if(MAGE_TARGET_IS_GPU)
      list(APPEND mage_ctest_args --parallel "${MAGE_GPU_TEST_JOBS}")
    endif()

    add_custom_target(check-mage
      COMMAND
        "${CMAKE_CTEST_COMMAND}" ${mage_ctest_args}
      DEPENDS
        mage
        mage-unittests-build
      USES_TERMINAL)
  else()
    add_custom_target(check-mage)
  endif()
endfunction()

# Adds a GPU leaf build rooted at build/<gpu_target> and exposes convenience
# targets to build or test that sub-build from the top-level build tree.
function(mage_add_gpu_subbuild gpu_target)
  set(subbuild_binary_dir "${CMAKE_BINARY_DIR}/${gpu_target}")
  set(config_target "configure-mage-${gpu_target}")

  ExternalProject_Add("${config_target}"
    PREFIX "${CMAKE_BINARY_DIR}/.superbuild/${gpu_target}"
    SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    BINARY_DIR "${subbuild_binary_dir}"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
      -DMAGE_INTERNAL_LEAF_BUILD=ON
      -DMAGE_INTERNAL_TARGET_TRIPLE=${gpu_target}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DMAGE_ENABLE_ASSERTIONS=${MAGE_ENABLE_ASSERTIONS}
      -DMAGE_LLVM_ROOT=${MAGE_LLVM_ROOT}
      -DLLVM_DIR=${LLVM_DIR}
      -DMAGE_GPU_TARGETS=
      -DMAGE_FORCE_AMDGPU_ARCH=${MAGE_FORCE_AMDGPU_ARCH}
      -DMAGE_FORCE_NVPTX_ARCH=${MAGE_FORCE_NVPTX_ARCH}
      -DMAGE_GPU_LOADER=${MAGE_GPU_LOADER}
      -DMAGE_GPU_LOADER_ARGS=${MAGE_GPU_LOADER_ARGS}
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    USES_TERMINAL_CONFIGURE TRUE)

  add_custom_target("mage-${gpu_target}"
    COMMAND
      "${CMAKE_COMMAND}" --build "${subbuild_binary_dir}" --target mage
    DEPENDS
      "${config_target}"
    USES_TERMINAL)

  add_custom_target("check-mage-${gpu_target}"
    COMMAND
      "${CMAKE_COMMAND}" --build "${subbuild_binary_dir}" --target check-mage
    DEPENDS
      "${config_target}"
    USES_TERMINAL)
endfunction()

function(mage_configure_build)
  if(MAGE_INTERNAL_LEAF_BUILD)
    mage_configure_leaf_build()
    return()
  endif()

  mage_validate_gpu_targets()

  set(MAGE_INTERNAL_TARGET_TRIPLE "default")
  mage_configure_leaf_build()

  set(mage_gpu_build_targets)

  foreach(gpu_target IN LISTS MAGE_GPU_TARGETS)
    if(gpu_target STREQUAL "")
      continue()
    endif()

    mage_add_gpu_subbuild("${gpu_target}")
    list(APPEND mage_gpu_build_targets "mage-${gpu_target}")
  endforeach()

  # The default root build should construct host artifacts plus every enabled
  # GPU leaf build. This avoids the surprising behavior where `ninja -C build`
  # only materializes the leaf directories without building their artifacts.
  add_custom_target(mage-all ALL)
  add_dependencies(mage-all mage)
  if(mage_gpu_build_targets)
    add_dependencies(mage-all ${mage_gpu_build_targets})
  endif()
endfunction()
