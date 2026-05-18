# Internal helper functions shared by Mage rule modules.

include_guard(GLOBAL)

set(MAGE_OBJECT_LIBRARY_TARGET_TYPE "MAGE_OBJECT_LIBRARY")
set(MAGE_LIBRARY_TARGET_TYPE "MAGE_LIBRARY")
set(MAGE_BITCODE_LIBRARY_TARGET_TYPE "MAGE_BITCODE_LIBRARY")

set(MAGE_SOURCE_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")

# ------------------------------------------------------------------------------
# Build helpers
# ------------------------------------------------------------------------------

function(_mage_get_normalized_builds out_var builds_list)
  set(normalized_builds)

  if(builds_list)
    foreach(build IN LISTS builds_list)
      string(STRIP "${build}" build)
      string(TOUPPER "${build}" build)

      if(NOT build STREQUAL "HOST" AND
        NOT build STREQUAL "GPU")
        message(FATAL_ERROR
          "unsupported build '${build}'; expected HOST and/or GPU")
      endif()

      list(APPEND normalized_builds "${build}")
    endforeach()
  else()
    list(APPEND normalized_builds HOST GPU)
  endif()

  list(REMOVE_DUPLICATES normalized_builds)
  set(${out_var} "${normalized_builds}" PARENT_SCOPE)
endfunction()

function(_mage_set_builds_for_target target_name builds_list)
  _mage_get_normalized_builds(builds "${builds_list}")

  set_property(GLOBAL PROPERTY
    "MAGE_BUILDS_FOR_${target_name}" "${builds}")
endfunction()

function(_mage_builds_include_current_build out_var builds_list)
  _mage_get_normalized_builds(builds "${builds_list}")

  if(MAGE_BUILD_IS_GPU)
    set(current_build GPU)
  else()
    set(current_build HOST)
  endif()

  if(current_build IN_LIST builds)
    set(${out_var} ON PARENT_SCOPE)
  else()
    set(${out_var} OFF PARENT_SCOPE)
  endif()
endfunction()

function(_mage_get_builds_for_target out_var target_name)
  get_property(builds GLOBAL PROPERTY "MAGE_BUILDS_FOR_${target_name}")

  if(NOT builds)
    set(builds)
  endif()

  set(${out_var} "${builds}" PARENT_SCOPE)
endfunction()

function(_mage_require_deps_in_current_build target_name deps_list)
  foreach(dep_target IN LISTS deps_list)
    if(NOT TARGET "${dep_target}")
      message(FATAL_ERROR
        "${target_name} depends on unknown target '${dep_target}'")
    endif()

    _mage_get_builds_for_target(dep_builds "${dep_target}")
    if(NOT dep_builds)
      message(FATAL_ERROR
        "${target_name} depends on '${dep_target}', but '${dep_target}' "
        "was not registered with Mage builds")
    endif()

    _mage_builds_include_current_build(dep_enabled "${dep_builds}")
    if(NOT dep_enabled)
      message(FATAL_ERROR
        "${target_name} depends on '${dep_target}', but '${dep_target}' "
        "is not available in the current build")
    endif()
  endforeach()
endfunction()

# ------------------------------------------------------------------------------
# Dependency helpers
# ------------------------------------------------------------------------------

function(_mage_require_deps_have_allowed_target_types
    target_name allowed_target_types deps_list)
  foreach(dep_target IN LISTS deps_list)
    if(NOT TARGET "${dep_target}")
      message(FATAL_ERROR
        "${target_name} depends on unknown target '${dep_target}'")
    endif()

    get_target_property(target_type "${dep_target}" MAGE_TARGET_TYPE)
    if(NOT target_type OR target_type STREQUAL "target_type-NOTFOUND")
      message(FATAL_ERROR
        "${target_name} depends on '${dep_target}', but '${dep_target}' "
        "does not have the MAGE_TARGET_TYPE property")
    endif()

    if(NOT target_type IN_LIST allowed_target_types)
      list(JOIN allowed_target_types ", " allowed_target_types_str)

      message(FATAL_ERROR
        "${target_name} depends on '${dep_target}', but '${dep_target}' "
        "has unsupported MAGE_TARGET_TYPE '${target_type}'; expected one of: "
        "${allowed_target_types_str}")
    endif()
  endforeach()
endfunction()

# ------------------------------------------------------------------------------
# Compile option helpers
# ------------------------------------------------------------------------------

function(_mage_get_common_compile_options out_var)
  set(compile_options
    -Wall
    -Wextra
    -Werror
    -Wconversion
    -Wno-sign-conversion
    -Wdeprecated
    -Wno-pedantic
    -Wimplicit-fallthrough
    -Wwrite-strings
    -Wextra-semi
    -Wnewline-eof
    -Wnonportable-system-include-path
    -Wthread-safety
    -Wglobal-constructors
    -fno-exceptions
    -fno-lax-vector-conversions
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-rtti)

  if(MAGE_BUILD_IS_GPU)
    list(APPEND compile_options
      --target=${MAGE_TARGET_TRIPLE}
      -nogpulib
      -fvisibility=hidden
      -fconvergent-functions
      -flto
      -Wno-multi-gpu)

    if(MAGE_BUILD_IS_AMDGPU)
      list(APPEND compile_options
        "SHELL:-Xclang -mcode-object-version=none")
    elseif(MAGE_BUILD_IS_NVPTX)
      list(APPEND compile_options
        -Wno-unknown-cuda-version)
    else()
      message(FATAL_ERROR
        "unsupported GPU target triple in _mage_get_common_compile_options: "
        "${MAGE_TARGET_TRIPLE}")
    endif()
  endif()

  set(${out_var} "${compile_options}" PARENT_SCOPE)
endfunction()

function(_mage_resolve_common_compile_options out_var)
  cmake_parse_arguments(OPTION_RESOLUTION
    "NO_COMMON_COMPILE_OPTIONS"
    ""
    "COMPILE_OPTIONS"
    ${ARGN})

  if(OPTION_RESOLUTION_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "_mage_resolve_common_compile_options received unexpected arguments: "
      "${OPTION_RESOLUTION_UNPARSED_ARGUMENTS}")
  endif()

  set(compile_options)
  if(NOT OPTION_RESOLUTION_NO_COMMON_COMPILE_OPTIONS)
    _mage_get_common_compile_options(common_compile_options)
    list(APPEND compile_options ${common_compile_options})
  endif()

  list(APPEND compile_options ${OPTION_RESOLUTION_COMPILE_OPTIONS})

  set(${out_var} "${compile_options}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# Link option helpers
# ------------------------------------------------------------------------------

# Resolves the GPU architecture used by GPU link-option helpers.
function(_mage_get_resolved_gpu_architecture out_var)
  if(NOT "${MAGE_GPU_ARCHITECTURE}" STREQUAL "")
    set(${out_var} "${MAGE_GPU_ARCHITECTURE}" PARENT_SCOPE)
    return()
  endif()

  if(MAGE_BUILD_IS_AMDGPU)
    message(FATAL_ERROR "No AMDGPU architecture was detected or provided")
  endif()

  if(MAGE_BUILD_IS_NVPTX)
    message(FATAL_ERROR "No NVPTX architecture was detected or provided")
  endif()

  message(FATAL_ERROR
    "unsupported GPU target triple in _mage_get_resolved_gpu_architecture: "
    "${MAGE_TARGET_TRIPLE}")
endfunction()

function(_mage_get_common_link_options out_var)
  set(link_options)

  if(MAGE_BUILD_IS_GPU)
    list(APPEND link_options
      --target=${MAGE_TARGET_TRIPLE}
      -flto)

    _mage_get_resolved_gpu_architecture(gpu_architecture)
    if(MAGE_BUILD_IS_AMDGPU)
      list(APPEND link_options -mcpu=${gpu_architecture})
    elseif(MAGE_BUILD_IS_NVPTX)
      list(APPEND link_options -march=${gpu_architecture})
    else()
      message(FATAL_ERROR
        "unsupported GPU target triple in _mage_get_common_link_options: "
        "${MAGE_TARGET_TRIPLE}")
    endif()
  endif()

  set(${out_var} "${link_options}" PARENT_SCOPE)
endfunction()

function(_mage_get_common_bitcode_link_options out_var)
  set(link_options
    -flto
    -r
    -nostdlib
    -Wl,--lto-emit-llvm)

  if(MAGE_BUILD_IS_GPU)
    list(APPEND link_options --target=${MAGE_TARGET_TRIPLE})

    _mage_get_resolved_gpu_architecture(gpu_architecture)

    if(MAGE_BUILD_IS_AMDGPU)
      list(APPEND link_options -mcpu=${gpu_architecture})
    elseif(MAGE_BUILD_IS_NVPTX)
      list(APPEND link_options -march=${gpu_architecture})
    else()
      message(FATAL_ERROR
        "unsupported GPU target triple in "
        "_mage_get_common_bitcode_link_options: ${MAGE_TARGET_TRIPLE}")
    endif()
  endif()

  set(${out_var} "${link_options}" PARENT_SCOPE)
endfunction()

function(_mage_resolve_common_link_options out_var)
  cmake_parse_arguments(OPTION_RESOLUTION
    "NO_COMMON_LINK_OPTIONS"
    ""
    "LINK_OPTIONS"
    ${ARGN})

  if(OPTION_RESOLUTION_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "_mage_resolve_common_link_options received unexpected arguments: "
      "${OPTION_RESOLUTION_UNPARSED_ARGUMENTS}")
  endif()

  set(link_options)
  if(NOT OPTION_RESOLUTION_NO_COMMON_LINK_OPTIONS)
    _mage_get_common_link_options(common_link_options)
    list(APPEND link_options ${common_link_options})
  endif()
  list(APPEND link_options ${OPTION_RESOLUTION_LINK_OPTIONS})

  set(${out_var} "${link_options}" PARENT_SCOPE)
endfunction()

function(_mage_resolve_common_bitcode_link_options out_var)
  cmake_parse_arguments(OPTION_RESOLUTION
    "NO_COMMON_LINK_OPTIONS"
    ""
    "LINK_OPTIONS"
    ${ARGN})

  if(OPTION_RESOLUTION_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "_mage_resolve_common_bitcode_link_options received unexpected "
      "arguments: ${OPTION_RESOLUTION_UNPARSED_ARGUMENTS}")
  endif()

  set(link_options)
  if(NOT OPTION_RESOLUTION_NO_COMMON_LINK_OPTIONS)
    _mage_get_common_bitcode_link_options(common_link_options)
    list(APPEND link_options ${common_link_options})
  endif()
  list(APPEND link_options
    ${OPTION_RESOLUTION_LINK_OPTIONS})

  set(${out_var} "${link_options}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# Library artifact registry helpers
# ------------------------------------------------------------------------------

function(_mage_append_registered_target property_name target_name)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR
      "cannot register non-existent target '${target_name}' "
      "in property '${property_name}'")
  endif()

  set_property(GLOBAL APPEND PROPERTY "${property_name}" "${target_name}")
endfunction()

function(_mage_register_archive_target target_name)
  _mage_append_registered_target(MAGE_ARCHIVE_TARGETS "${target_name}")
endfunction()

function(_mage_register_bitcode_target target_name)
  _mage_append_registered_target(MAGE_BITCODE_TARGETS "${target_name}")
endfunction()

# ------------------------------------------------------------------------------
# Object file collection helpers
# ------------------------------------------------------------------------------

function(_mage_collect_object_lib_targets_from_target out_var target_name)
  get_property(visiting GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITING)
  get_property(visited GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITED)

  if(target_name IN_LIST visiting)
    list(APPEND visiting "${target_name}")
    list(JOIN visiting " -> " cycle)
    message(FATAL_ERROR
      "dependency cycle detected while collecting object libraries: ${cycle}")
  endif()

  if(target_name IN_LIST visited)
    set(${out_var} "" PARENT_SCOPE)
    return()
  endif()

  list(APPEND visiting "${target_name}")
  set_property(GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITING "${visiting}")

  set(all_object_lib_targets)

  get_target_property(target_type "${target_name}" MAGE_TARGET_TYPE)
  if(target_type STREQUAL "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}")
    list(APPEND all_object_lib_targets "${target_name}")

    get_target_property(deps_list "${target_name}" MAGE_DEPS)
    if(NOT deps_list OR deps_list STREQUAL "deps_list-NOTFOUND")
      set(deps_list)
    endif()

    foreach(dep_target IN LISTS deps_list)
      _mage_collect_object_lib_targets_from_target(
        object_lib_targets "${dep_target}")
      list(APPEND all_object_lib_targets ${object_lib_targets})
    endforeach()
  endif()

  get_property(visiting GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITING)
  list(REMOVE_ITEM visiting "${target_name}")
  set_property(GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITING "${visiting}")

  get_property(visited GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITED)
  list(APPEND visited "${target_name}")
  set_property(GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITED "${visited}")

  list(REMOVE_DUPLICATES all_object_lib_targets)
  set(${out_var} "${all_object_lib_targets}" PARENT_SCOPE)
endfunction()

function(_mage_collect_object_lib_targets_from_deps out_var deps_list)
  set_property(GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITING "")
  set_property(GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITED "")

  set(all_object_lib_targets)

  foreach(dep_target IN LISTS deps_list)
    _mage_collect_object_lib_targets_from_target(
      object_lib_targets "${dep_target}")
    list(APPEND all_object_lib_targets ${object_lib_targets})
  endforeach()

  set_property(GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITING "")
  set_property(GLOBAL PROPERTY MAGE_OBJECT_LIBS_VISITED "")

  list(REMOVE_DUPLICATES all_object_lib_targets)
  set(${out_var} "${all_object_lib_targets}" PARENT_SCOPE)
endfunction()

# Precondition: callers must validate that deps_list contains only supported
# Mage targets before collecting their object files.
function(_mage_get_all_object_files_from_deps out_var deps_list)
  _mage_collect_object_lib_targets_from_deps(object_lib_targets "${deps_list}")

  set(all_object_files "")
  foreach(object_lib_target IN LISTS object_lib_targets)
    list(APPEND all_object_files $<TARGET_OBJECTS:${object_lib_target}>)
  endforeach()

  set(${out_var} "${all_object_files}" PARENT_SCOPE)
endfunction()
