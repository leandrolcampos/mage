# Configures CUDA dependencies.

include_guard(GLOBAL)

# Keeps CUDA Driver API dependents configurable, but fails when they are built.
function(_mage_configure_unavailable_cuda_driver_target)
  string(CONCAT reason ${ARGV})

  if(NOT TARGET MageCUDADriverUnavailable)
    add_custom_target(MageCUDADriverUnavailable
      COMMAND
        "${CMAKE_COMMAND}" -E echo "error: ${reason}"
      COMMAND
        "${CMAKE_COMMAND}" -E false
      VERBATIM)
  endif()

  add_dependencies(MageCUDADriver MageCUDADriverUnavailable)
endfunction()

# Configures the CUDA Driver API target when it is available for this build.
function(_mage_configure_cuda_driver)
  if(TARGET MageCUDADriver)
    return()
  endif()

  add_library(MageCUDADriver INTERFACE)
  add_library(Mage::CUDADriver ALIAS MageCUDADriver)

  if(MAGE_BUILD_KIND STREQUAL "GPU")
    _mage_configure_unavailable_cuda_driver_target(
      "CUDA Driver API is unavailable in GPU builds")

    set(MAGE_CUDA_DRIVER_FOUND OFF CACHE INTERNAL
      "Whether the CUDA Driver API is available in the current build" FORCE)
    set(MAGE_CUDA_BACKEND_ENABLED OFF CACHE INTERNAL
      "Whether the CUDA backend is enabled in the current build" FORCE)
    return()
  endif()

  if(TARGET CUDA::cuda_driver)
    target_link_libraries(MageCUDADriver INTERFACE
      CUDA::cuda_driver)

    set(MAGE_CUDA_DRIVER_FOUND ON CACHE INTERNAL
      "Whether the CUDA Driver API is available in the current build" FORCE)
    set(MAGE_CUDA_BACKEND_ENABLED ON CACHE INTERNAL
      "Whether the CUDA backend is enabled in the current build" FORCE)
  else()
    message(STATUS
      "CUDA Driver API was not found; CUDA backend is unavailable")

    _mage_configure_unavailable_cuda_driver_target(
      "this target requires the CUDA Driver API (install the CUDA Toolkit "
      "and NVIDIA driver)")

    set(MAGE_CUDA_DRIVER_FOUND OFF CACHE INTERNAL
      "Whether the CUDA Driver API is available in the current build" FORCE)
    set(MAGE_CUDA_BACKEND_ENABLED OFF CACHE INTERNAL
      "Whether the CUDA backend is enabled in the current build" FORCE)
  endif()
endfunction()

# Configures CUDA package components used by Mage.
function(mage_configure_cuda)
  if(NOT DEFINED MAGE_BUILD_KIND)
    message(FATAL_ERROR
      "mage_configure_cuda() requires MAGE_BUILD_KIND to be set")
  endif()

  if(MAGE_BUILD_KIND STREQUAL "HOST" OR MAGE_TARGET_ARCH_IS_NVPTX)
    find_package(CUDAToolkit QUIET)

    if(CUDAToolkit_FOUND)
      message(STATUS
        "Found CUDA Toolkit: ${CUDAToolkit_LIBRARY_DIR} "
        "(found version \"${CUDAToolkit_VERSION}\")")
    else()
      message(STATUS
        "CUDA Toolkit was not found; dependent components are unavailable")
    endif()
  endif()

  _mage_configure_cuda_driver()
endfunction()
