# Configures ROCm dependencies.

include_guard(GLOBAL)

# Keeps HIP runtime dependents configurable, but fails when they are built.
function(_mage_configure_unavailable_hip_runtime_target)
  string(CONCAT reason ${ARGV})

  if(NOT TARGET MageHIPRuntimeUnavailable)
    add_custom_target(MageHIPRuntimeUnavailable
      COMMAND
        "${CMAKE_COMMAND}" -E echo "error: ${reason}"
      COMMAND
        "${CMAKE_COMMAND}" -E false
      VERBATIM)
  endif()

  add_dependencies(MageHIPRuntime MageHIPRuntimeUnavailable)
endfunction()

# Configures the HIP runtime target when it is available for this build.
function(_mage_configure_hip_runtime)
  if(TARGET MageHIPRuntime)
    return()
  endif()

  add_library(MageHIPRuntime INTERFACE)
  add_library(Mage::HIPRuntime ALIAS MageHIPRuntime)

  if(MAGE_BUILD_KIND STREQUAL "GPU")
    _mage_configure_unavailable_hip_runtime_target(
      "HIP runtime is unavailable in GPU builds")

    set(MAGE_HIP_RUNTIME_FOUND OFF CACHE INTERNAL
      "Whether the HIP runtime is available in the current build" FORCE)
    set(MAGE_HIP_BACKEND_ENABLED OFF CACHE INTERNAL
      "Whether the HIP backend is enabled in the current build" FORCE)
    return()
  endif()

  find_package(hip CONFIG QUIET
    HINTS
      ${CMAKE_INSTALL_PREFIX}
    PATHS
      /opt/rocm)

  if(hip_FOUND)
    message(STATUS
      "Found HIP package: ${HIP_PACKAGE_PREFIX_DIR} "
      "(found version \"${hip_VERSION}\", platform \"${HIP_PLATFORM}\")")
  else()
    message(STATUS
      "HIP package was not found; AMD HIP runtime is unavailable")

    _mage_configure_unavailable_hip_runtime_target(
      "this target requires the AMD HIP runtime (install ROCm)")

    set(MAGE_HIP_RUNTIME_FOUND OFF CACHE INTERNAL
      "Whether the HIP runtime is available in the current build" FORCE)
    set(MAGE_HIP_BACKEND_ENABLED OFF CACHE INTERNAL
      "Whether the HIP backend is enabled in the current build" FORCE)
    return()
  endif()

  if(TARGET hip::host AND DEFINED HIP_PLATFORM AND HIP_PLATFORM STREQUAL "amd")
    target_link_libraries(MageHIPRuntime INTERFACE
      hip::host)

    set(MAGE_HIP_RUNTIME_FOUND ON CACHE INTERNAL
      "Whether the HIP runtime is available in the current build" FORCE)
    set(MAGE_HIP_BACKEND_ENABLED ON CACHE INTERNAL
      "Whether the HIP backend is enabled in the current build" FORCE)
  else()
    if(TARGET hip::host AND DEFINED HIP_PLATFORM)
      message(STATUS
        "HIP package is configured for platform '${HIP_PLATFORM}', not "
        "'amd'; AMD HIP runtime is unavailable")
    else()
      message(STATUS
        "HIP package does not provide an AMD HIP runtime; "
        "dependent targets are unavailable")
    endif()

    _mage_configure_unavailable_hip_runtime_target(
      "this target requires the AMD HIP runtime (install ROCm)")

    set(MAGE_HIP_RUNTIME_FOUND OFF CACHE INTERNAL
      "Whether the HIP runtime is available in the current build" FORCE)
    set(MAGE_HIP_BACKEND_ENABLED OFF CACHE INTERNAL
      "Whether the HIP backend is enabled in the current build" FORCE)
  endif()
endfunction()

# Configures ROCm package components used by Mage.
function(mage_configure_rocm)
  if(NOT DEFINED MAGE_BUILD_KIND)
    message(FATAL_ERROR
      "mage_configure_rocm() requires MAGE_BUILD_KIND to be set")
  endif()

  _mage_configure_hip_runtime()
endfunction()
