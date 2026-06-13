# Configures the MPFR dependency.

include_guard(GLOBAL)

function(_mage_configure_unavailable_mpfr_target reason)
  add_custom_target(MageMPFRUnavailable
    COMMAND
      "${CMAKE_COMMAND}" -E echo "error: ${reason}"
    COMMAND
      "${CMAKE_COMMAND}" -E false
    VERBATIM)

  add_dependencies(MageMPFR MageMPFRUnavailable)
endfunction()

function(mage_configure_mpfr)
  if(TARGET MageMPFR)
    return()
  endif()

  if(NOT DEFINED MAGE_BUILD_KIND)
    message(FATAL_ERROR
      "mage_configure_mpfr() requires MAGE_BUILD_KIND to be set")
  endif()

  add_library(MageMPFR INTERFACE)
  add_library(Mage::MPFR ALIAS MageMPFR)

  if(MAGE_BUILD_KIND STREQUAL "GPU")
    _mage_configure_unavailable_mpfr_target(
      "MPFR is unavailable in GPU builds")

    set(MAGE_MPFR_FOUND OFF CACHE INTERNAL
      "Whether MPFR is available in the current build" FORCE)
    return()
  endif()

  find_path(mage_mpfr_include_dir
    NAMES mpfr.h)
  find_library(mage_mpfr_library
    NAMES mpfr)
  find_library(mage_gmp_library
    NAMES gmp)

  if(mage_mpfr_include_dir AND mage_mpfr_library AND mage_gmp_library)
    target_include_directories(MageMPFR SYSTEM INTERFACE
      "${mage_mpfr_include_dir}")

    target_link_libraries(MageMPFR INTERFACE
      "${mage_mpfr_library}"
      "${mage_gmp_library}")

    set(MAGE_MPFR_FOUND ON CACHE INTERNAL
      "Whether MPFR is available in the current build" FORCE)
  else()
    message(STATUS
      "MPFR was not found; dependent targets are unavailable")

    _mage_configure_unavailable_mpfr_target(
      "this target requires MPFR (on Ubuntu, install libmpfr-dev)")

    set(MAGE_MPFR_FOUND OFF CACHE INTERNAL
      "Whether MPFR is available in the current build" FORCE)
  endif()

  unset(mage_mpfr_include_dir CACHE)
  unset(mage_mpfr_library CACHE)
  unset(mage_gmp_library CACHE)
endfunction()
