# CMake module for configuring host build and optional GPU sub-builds.
#
# It resolves per-target build context, decides whether GPU unit tests
# are buildable on the current host, and creates GPU leaf builds using
# ExternalProject.

include_guard(GLOBAL)

include(CheckCXXCompilerFlag)
include(ExternalProject)
include(MageTools)
include(MageRules)

function(mage_validate_gpu_targets)
  set(seen_gpu_targets)

  foreach(gpu_target IN LISTS MAGE_GPU_TARGETS)
    if(gpu_target STREQUAL "")
      continue()
    endif()

    if(NOT gpu_target STREQUAL "amdgcn-amd-amdhsa" AND
       NOT gpu_target STREQUAL "nvptx64-nvidia-cuda")
      message(FATAL_ERROR
        "Unsupported GPU target '${gpu_target}' in MAGE_GPU_TARGETS")
    endif()

    if(gpu_target IN_LIST seen_gpu_targets)
      message(FATAL_ERROR
        "Duplicate GPU target '${gpu_target}' in MAGE_GPU_TARGETS")
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
      "Unsupported GPU target in mage_check_native_gpu_arch_support: "
      "${gpu_target}")
  endif()

  set(CMAKE_REQUIRED_FLAGS "${old_required_flags}")
  set(CMAKE_TRY_COMPILE_TARGET_TYPE "${old_try_compile_target_type}")

  set(${out_var} "${HOST_CAN_RESOLVE_GPU_NATIVE_ARCH}" PARENT_SCOPE)
endfunction()

# Resolves the build context for the current leaf build, including the target
# triple, backend classification, optional GPU architecture, and test command.
# Exports the resolved values to the parent scope.
function(mage_resolve_leaf_context)
  set(mage_target_triple "${MAGE_INTERNAL_TARGET_TRIPLE}")
  set(mage_target_is_gpu OFF)
  set(mage_target_is_amdgpu OFF)
  set(mage_target_is_nvptx OFF)
  set(mage_gpu_architecture "")
  set(mage_test_cmd "${MAGE_TEST_CMD}")

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

    mage_resolve_llvm_link()
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

    mage_resolve_llvm_link()
  else()
    message(FATAL_ERROR
      "Unsupported target triple in mage_resolve_leaf_context: "
      "${mage_target_triple}")
  endif()

  if(mage_target_is_gpu AND mage_test_cmd STREQUAL "")
    if(MAGE_GPU_LOADER STREQUAL "")
      message(FATAL_ERROR
        "GPU tests require MAGE_GPU_LOADER or MAGE_TEST_CMD")
    endif()

    if(MAGE_GPU_LOADER_ARGS STREQUAL "")
      set(mage_test_cmd "${MAGE_GPU_LOADER}")
    else()
      set(mage_test_cmd "${MAGE_GPU_LOADER} ${MAGE_GPU_LOADER_ARGS}")
    endif()
  endif()

  set(MAGE_TARGET_TRIPLE "${mage_target_triple}" PARENT_SCOPE)
  set(MAGE_TARGET_IS_GPU "${mage_target_is_gpu}" PARENT_SCOPE)
  set(MAGE_TARGET_IS_AMDGPU "${mage_target_is_amdgpu}" PARENT_SCOPE)
  set(MAGE_TARGET_IS_NVPTX "${mage_target_is_nvptx}" PARENT_SCOPE)
  set(MAGE_GPU_ARCHITECTURE "${mage_gpu_architecture}" PARENT_SCOPE)
  set(MAGE_TEST_CMD "${mage_test_cmd}" PARENT_SCOPE)
endfunction()

function(mage_should_build_unit_tests out_var)
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
      "Skipping unit tests for amdgcn-amd-amdhsa because no AMDGPU "
      "architecture was detected or provided")
    set(${out_var} OFF PARENT_SCOPE)
    return()
  endif()

  if(MAGE_TARGET_IS_NVPTX)
    message(STATUS
      "Skipping unit tests for nvptx64-nvidia-cuda because no NVPTX "
      "architecture was detected or provided")
    set(${out_var} OFF PARENT_SCOPE)
    return()
  endif()

  message(FATAL_ERROR
    "Unsupported target triple in mage_should_build_unit_tests: "
    "${MAGE_TARGET_TRIPLE}")
endfunction()

function(mage_configure_leaf_build)
  mage_resolve_leaf_context()

  add_subdirectory(lib)

  mage_should_build_unit_tests(mage_build_unit_tests)
  if(mage_build_unit_tests)
    add_subdirectory(unittests)
  else()
    add_custom_target(check-mage)
  endif()
endfunction()

# Adds a GPU leaf build rooted at build/<gpu_target> and exposes convenience
# targets to build or test that sub-build from the top-level build tree.
function(mage_add_gpu_subbuild gpu_target)
  set(subbuild_binary_dir "${CMAKE_BINARY_DIR}/${gpu_target}")

  ExternalProject_Add("mage-${gpu_target}"
    PREFIX "${CMAKE_BINARY_DIR}/.superbuild/${gpu_target}"
    SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    BINARY_DIR "${subbuild_binary_dir}"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
      -DMAGE_INTERNAL_LEAF_BUILD=ON
      -DMAGE_INTERNAL_TARGET_TRIPLE=${gpu_target}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DMAGE_GPU_TARGETS=
      -DMAGE_FORCE_AMDGPU_ARCH=${MAGE_FORCE_AMDGPU_ARCH}
      -DMAGE_FORCE_NVPTX_ARCH=${MAGE_FORCE_NVPTX_ARCH}
      -DLLVM_LIT=${LLVM_LIT}
      -DLLVM_LINK=${LLVM_LINK}
      -DMAGE_GPU_LOADER=${MAGE_GPU_LOADER}
      -DMAGE_GPU_LOADER_ARGS=${MAGE_GPU_LOADER_ARGS}
      -DMAGE_TEST_CMD=${MAGE_TEST_CMD}
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    USES_TERMINAL_CONFIGURE TRUE)

  add_custom_target("build-mage-${gpu_target}"
    COMMAND "${CMAKE_COMMAND}" --build "${subbuild_binary_dir}"
    DEPENDS "mage-${gpu_target}"
    USES_TERMINAL)

  add_custom_target("check-mage-${gpu_target}"
    COMMAND "${CMAKE_COMMAND}" --build "${subbuild_binary_dir}" --target check-mage
    DEPENDS "mage-${gpu_target}"
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

  foreach(gpu_target IN LISTS MAGE_GPU_TARGETS)
    if(gpu_target STREQUAL "")
      continue()
    endif()

    mage_add_gpu_subbuild("${gpu_target}")
  endforeach()
endfunction()
