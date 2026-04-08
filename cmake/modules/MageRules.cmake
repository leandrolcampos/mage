# CMake module for defining Mage libraries and unit tests.
#
# It parses and validates target declarations, enforces Mage-specific
# dependency rules and structural constraints, and applies the common
# build logic used to construct library and test targets.

include_guard(GLOBAL)

set(MAGE_OBJECT_LIBRARY_TARGET_TYPE "MAGE_OBJECT_LIBRARY")
set(MAGE_LIBRARY_TARGET_TYPE "MAGE_LIBRARY")

# ------------------------------------------------------------------------------
# Target-kind normalization and current-leaf selection helpers
# ------------------------------------------------------------------------------

# Parses TARGET_KINDS, applies the default target kinds when TARGET_KINDS is
# omitted, validates the values, and returns a deduplicated list.
function(mage_get_normalized_target_kinds out_var)
  cmake_parse_arguments(TARGET_KIND_ARGS "" "" "TARGET_KINDS" ${ARGN})

  if(TARGET_KIND_ARGS_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_get_normalized_target_kinds received unexpected arguments: "
      "${TARGET_KIND_ARGS_UNPARSED_ARGUMENTS}")
  endif()

  if(TARGET_KIND_ARGS_TARGET_KINDS)
    set(requested_target_kinds ${TARGET_KIND_ARGS_TARGET_KINDS})
  else()
    set(requested_target_kinds HOST GPU)
  endif()

  foreach(requested_target_kind IN LISTS requested_target_kinds)
    if(NOT requested_target_kind STREQUAL "HOST" AND
       NOT requested_target_kind STREQUAL "GPU")
      message(FATAL_ERROR
        "unsupported target kind '${requested_target_kind}'; "
        "expected HOST and/or GPU")
    endif()
  endforeach()

  list(REMOVE_DUPLICATES requested_target_kinds)
  set(${out_var} "${requested_target_kinds}" PARENT_SCOPE)
endfunction()

# Returns whether the requested target kinds include the current build leaf.
function(mage_target_matches_current_leaf out_var)
  mage_get_normalized_target_kinds(requested_target_kinds ${ARGN})

  if(MAGE_TARGET_IS_GPU)
    if("GPU" IN_LIST requested_target_kinds)
      set(${out_var} TRUE PARENT_SCOPE)
    else()
      set(${out_var} FALSE PARENT_SCOPE)
    endif()
    return()
  endif()

  if("HOST" IN_LIST requested_target_kinds)
    set(${out_var} TRUE PARENT_SCOPE)
  else()
    set(${out_var} FALSE PARENT_SCOPE)
  endif()
endfunction()

# Returns whether the requested target kinds include GPU.
function(mage_target_supports_gpu out_var)
  mage_get_normalized_target_kinds(requested_target_kinds ${ARGN})

  if("GPU" IN_LIST requested_target_kinds)
    set(${out_var} TRUE PARENT_SCOPE)
  else()
    set(${out_var} FALSE PARENT_SCOPE)
  endif()
endfunction()

# ------------------------------------------------------------------------------
# Common compile and link option helpers
# ------------------------------------------------------------------------------

function(mage_get_common_compile_options out_var)
  set(compile_options
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -fno-exceptions
    -fno-rtti)

  if(MAGE_TARGET_IS_GPU)
    list(APPEND compile_options
      --target=${MAGE_TARGET_TRIPLE}
      -flto)
  endif()

  set(${out_var} "${compile_options}" PARENT_SCOPE)
endfunction()

# Resolves the effective GPU architecture used by GPU link-option helpers.
function(mage_get_resolved_gpu_arch out_var)
  if(NOT MAGE_GPU_ARCHITECTURE STREQUAL "")
    set(${out_var} "${MAGE_GPU_ARCHITECTURE}" PARENT_SCOPE)
    return()
  endif()

  if(MAGE_TARGET_IS_AMDGPU)
    message(FATAL_ERROR "No AMDGPU architecture was detected or provided")
  endif()

  if(MAGE_TARGET_IS_NVPTX)
    message(FATAL_ERROR "No NVPTX architecture was detected or provided")
  endif()

  message(FATAL_ERROR
    "unsupported GPU target in mage_get_resolved_gpu_arch: "
    "${MAGE_TARGET_TRIPLE}")
endfunction()

function(mage_get_common_link_options out_var)
  set(link_options)

  if(MAGE_TARGET_IS_GPU)
    mage_get_resolved_gpu_arch(mage_gpu_arch)

    list(APPEND link_options
      --target=${MAGE_TARGET_TRIPLE}
      -flto)

    if(MAGE_TARGET_IS_AMDGPU)
      list(APPEND link_options -mcpu=${mage_gpu_arch})
    elseif(MAGE_TARGET_IS_NVPTX)
      list(APPEND link_options -march=${mage_gpu_arch})
    else()
      message(FATAL_ERROR
        "unsupported GPU target in mage_get_common_link_options: "
        "${MAGE_TARGET_TRIPLE}")
    endif()
  endif()

  set(${out_var} "${link_options}" PARENT_SCOPE)
endfunction()

function(mage_get_common_bitcode_link_options out_var)
  if(NOT MAGE_TARGET_IS_GPU)
    set(${out_var} "" PARENT_SCOPE)
    return()
  endif()

  mage_get_resolved_gpu_arch(mage_gpu_arch)

  set(link_options
    --target=${MAGE_TARGET_TRIPLE}
    -flto
    -r
    -nostdlib
    -Wl,--lto-emit-llvm)

  if(MAGE_TARGET_IS_AMDGPU)
    list(APPEND link_options -mcpu=${mage_gpu_arch})
  elseif(MAGE_TARGET_IS_NVPTX)
    list(APPEND link_options -march=${mage_gpu_arch})
  else()
    message(FATAL_ERROR
      "unsupported GPU target in mage_get_common_bitcode_link_options: "
      "${MAGE_TARGET_TRIPLE}")
  endif()

  set(${out_var} "${link_options}" PARENT_SCOPE)
endfunction()

# These options are not returned by mage_get_common_link_options because they
# are specific to GPU executables intended to run under MAGE_GPU_LOADER.
function(mage_get_common_gpu_loader_link_options out_var)
  if(NOT MAGE_TARGET_IS_GPU)
    set(${out_var} "" PARENT_SCOPE)
    return()
  endif()

  set(loader_link_options
    -stdlib
    -startfiles)
  set(${out_var} "${loader_link_options}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# Effective option resolution helpers
# ------------------------------------------------------------------------------

function(mage_resolve_common_compile_options out_var)
  cmake_parse_arguments(OPTION_RESOLUTION
    "NO_COMMON_COMPILE_OPTIONS"
    ""
    "COMPILE_OPTIONS"
    ${ARGN})

  if(OPTION_RESOLUTION_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_resolve_common_compile_options received unexpected arguments: "
      "${OPTION_RESOLUTION_UNPARSED_ARGUMENTS}")
  endif()

  set(compile_options)
  if(NOT OPTION_RESOLUTION_NO_COMMON_COMPILE_OPTIONS)
    mage_get_common_compile_options(common_compile_options)
    list(APPEND compile_options ${common_compile_options})
  endif()
  list(APPEND compile_options ${OPTION_RESOLUTION_COMPILE_OPTIONS})

  set(${out_var} "${compile_options}" PARENT_SCOPE)
endfunction()

function(mage_resolve_common_link_options out_var)
  cmake_parse_arguments(OPTION_RESOLUTION
    "NO_COMMON_LINK_OPTIONS"
    ""
    "LINK_OPTIONS"
    ${ARGN})

  if(OPTION_RESOLUTION_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_resolve_common_link_options received unexpected arguments: "
      "${OPTION_RESOLUTION_UNPARSED_ARGUMENTS}")
  endif()

  set(link_options)
  if(NOT OPTION_RESOLUTION_NO_COMMON_LINK_OPTIONS)
    mage_get_common_link_options(common_link_options)
    list(APPEND link_options ${common_link_options})
  endif()
  list(APPEND link_options ${OPTION_RESOLUTION_LINK_OPTIONS})

  set(${out_var} "${link_options}" PARENT_SCOPE)
endfunction()

function(mage_resolve_common_bitcode_link_options out_var)
  cmake_parse_arguments(OPTION_RESOLUTION
    "NO_COMMON_BITCODE_LINK_OPTIONS"
    ""
    "BITCODE_LINK_OPTIONS"
    ${ARGN})

  if(OPTION_RESOLUTION_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_resolve_common_bitcode_link_options received unexpected arguments: "
      "${OPTION_RESOLUTION_UNPARSED_ARGUMENTS}")
  endif()

  set(bitcode_link_options)
  if(NOT OPTION_RESOLUTION_NO_COMMON_BITCODE_LINK_OPTIONS)
    mage_get_common_bitcode_link_options(common_bitcode_link_options)
    list(APPEND bitcode_link_options ${common_bitcode_link_options})
  endif()
  list(APPEND bitcode_link_options
    ${OPTION_RESOLUTION_BITCODE_LINK_OPTIONS})

  set(${out_var} "${bitcode_link_options}" PARENT_SCOPE)
endfunction()

function(mage_resolve_common_gpu_loader_link_options out_var)
  cmake_parse_arguments(OPTION_RESOLUTION
    "NO_COMMON_GPU_LOADER_LINK_OPTIONS"
    ""
    "GPU_LOADER_LINK_OPTIONS"
    ${ARGN})

  if(OPTION_RESOLUTION_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_resolve_common_gpu_loader_link_options received "
      "unexpected arguments: "
      "${OPTION_RESOLUTION_UNPARSED_ARGUMENTS}")
  endif()

  set(gpu_loader_link_options)
  if(NOT OPTION_RESOLUTION_NO_COMMON_GPU_LOADER_LINK_OPTIONS)
    mage_get_common_gpu_loader_link_options(common_gpu_loader_link_options)
    list(APPEND gpu_loader_link_options ${common_gpu_loader_link_options})
  endif()
  list(APPEND gpu_loader_link_options
    ${OPTION_RESOLUTION_GPU_LOADER_LINK_OPTIONS})

  set(${out_var} "${gpu_loader_link_options}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# Registered library target helpers
# ------------------------------------------------------------------------------

function(mage_append_registered_target property_name target_name)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR
      "Cannot register non-existent target '${target_name}' "
      "in property '${property_name}'")
  endif()

  set_property(GLOBAL APPEND PROPERTY "${property_name}" "${target_name}")
endfunction()

function(mage_register_archive_target target_name)
  mage_append_registered_target(MAGE_ARCHIVE_TARGETS "${target_name}")
endfunction()

function(mage_register_bitcode_target target_name)
  mage_append_registered_target(MAGE_BITCODE_TARGETS "${target_name}")
endfunction()

# ------------------------------------------------------------------------------
# Mage target property and declared-target helpers
# ------------------------------------------------------------------------------

function(mage_register_declared_target_kinds target_name)
  mage_get_normalized_target_kinds(normalized_target_kinds TARGET_KINDS ${ARGN})
  set_property(GLOBAL PROPERTY "MAGE_DECLARED_TARGET_KINDS_${target_name}"
    "${normalized_target_kinds}")
endfunction()

function(mage_get_declared_target_kinds out_var target_name)
  get_property(
    value
    GLOBAL PROPERTY "MAGE_DECLARED_TARGET_KINDS_${target_name}"
  )
  if(value STREQUAL "value-NOTFOUND")
    set(value "")
  endif()
  set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

function(mage_get_target_metadata out_var target_name property_name)
  get_target_property(value ${target_name} "${property_name}")
  if(value STREQUAL "value-NOTFOUND")
    set(value "")
  endif()
  set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# Dependency graph and validation helpers
# ------------------------------------------------------------------------------

# Recursively validates the transitive DEPENDS graph of a Mage object/library
# target.
#
# This walk enforces three invariants for each reachable dependency:
# 1. the target exists and is available in the current leaf;
# 2. the target is a structurally pure Mage object/library target; and
# 3. the DEPENDS graph is acyclic.
function(mage_validate_library_dep_target_impl owner_target dep_target)
  get_property(visiting GLOBAL PROPERTY MAGE_VISITING_LIBRARY_DEP_TARGETS)
  if("${dep_target}" IN_LIST visiting)
    set(cycle_path "${visiting}")
    list(APPEND cycle_path "${dep_target}")
    list(JOIN cycle_path " -> " cycle_path_str)

    message(FATAL_ERROR
      "${owner_target} has a cycle in DEPENDS: ${cycle_path_str}")
  endif()

  get_property(visited GLOBAL PROPERTY MAGE_VISITED_LIBRARY_DEP_TARGETS)
  if("${dep_target}" IN_LIST visited)
    return()
  endif()

  if(NOT TARGET "${dep_target}")
    mage_get_declared_target_kinds(declared_target_kinds "${dep_target}")

    if(declared_target_kinds)
      if(MAGE_TARGET_IS_GPU)
        set(current_target_kind "GPU")
      else()
        set(current_target_kind "HOST")
      endif()

      list(JOIN declared_target_kinds ", " declared_target_kinds_str)

      message(FATAL_ERROR
        "${owner_target} depends on '${dep_target}', but '${dep_target}' "
        "is not available in the current ${current_target_kind} leaf. "
        "It was declared with TARGET_KINDS ${declared_target_kinds_str}")
    endif()

    message(FATAL_ERROR
      "${owner_target} depends on unknown target '${dep_target}'. "
      "Internal Mage dependencies must be defined before use")
  endif()

  # Track the active DFS stack separately from the completed nodes so that we
  # can diagnose real cycles while still skipping shared acyclic subgraphs.
  list(APPEND visiting "${dep_target}")
  set_property(GLOBAL PROPERTY MAGE_VISITING_LIBRARY_DEP_TARGETS "${visiting}")

  mage_get_target_metadata(target_type "${dep_target}" MAGE_TARGET_TYPE)
  if(NOT target_type STREQUAL "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}" AND
     NOT target_type STREQUAL "${MAGE_LIBRARY_TARGET_TYPE}")
    message(FATAL_ERROR
      "${owner_target} depends on '${dep_target}', but '${dep_target}' "
      "is not a Mage object or Mage library target. "
      "Use LINK_LIBRARIES for external libraries")
  endif()

  mage_get_target_metadata(
    dep_link_libraries "${dep_target}" MAGE_LINK_LIBRARIES)
  if(dep_link_libraries)
    list(JOIN dep_link_libraries ", " dep_link_libraries_str)

    message(FATAL_ERROR
      "${owner_target} depends on '${dep_target}', but '${dep_target}' "
      "has LINK_LIBRARIES set to ${dep_link_libraries_str}. "
      "Targets used in DEPENDS must be structurally pure Mage targets")
  endif()

  mage_get_target_metadata(nested_deps "${dep_target}" MAGE_DEPENDS)
  foreach(nested_dep IN LISTS nested_deps)
    mage_validate_library_dep_target_impl("${owner_target}" "${nested_dep}")
  endforeach()

  list(POP_BACK visiting)
  set_property(GLOBAL PROPERTY MAGE_VISITING_LIBRARY_DEP_TARGETS "${visiting}")

  list(APPEND visited "${dep_target}")
  set_property(GLOBAL PROPERTY MAGE_VISITED_LIBRARY_DEP_TARGETS "${visited}")
endfunction()

# Validates that the direct DEPENDS list of a Mage object/library target forms a
# valid transitive dependency graph.
#
# The graph must be acyclic, leaf-available, and composed only of structurally
# pure Mage object/library targets.
function(mage_validate_library_dep_targets owner_target)
  set_property(GLOBAL PROPERTY MAGE_VISITING_LIBRARY_DEP_TARGETS "")
  set_property(GLOBAL PROPERTY MAGE_VISITED_LIBRARY_DEP_TARGETS "")

  foreach(dep_target IN LISTS ARGN)
    mage_validate_library_dep_target_impl("${owner_target}" "${dep_target}")
  endforeach()

  set_property(GLOBAL PROPERTY MAGE_VISITING_LIBRARY_DEP_TARGETS "")
  set_property(GLOBAL PROPERTY MAGE_VISITED_LIBRARY_DEP_TARGETS "")
endfunction()

# Collects the transitive object-library closure for a Mage target.
#
# This helper assumes that mage_validate_library_dep_targets has already
# validated that MAGE_DEPENDS forms an acyclic graph of structurally pure Mage
# object/library targets in the current leaf.
#
# If the input is an object library, the result includes the target itself.
# If the input is a Mage library, the result includes only the object-library
# targets reachable through MAGE_DEPENDS.
function(mage_collect_object_targets target_name out_var)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR
      "unknown target '${target_name}' in mage_collect_object_targets")
  endif()

  mage_get_target_metadata(target_type "${target_name}" MAGE_TARGET_TYPE)

  if(target_type STREQUAL "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}")
    set(all_object_targets "${target_name}")
    mage_get_target_metadata(direct_deps "${target_name}" MAGE_DEPENDS)

    foreach(direct_dep IN LISTS direct_deps)
      mage_collect_object_targets("${direct_dep}" dep_object_targets)
      list(APPEND all_object_targets ${dep_object_targets})
    endforeach()

    list(REMOVE_DUPLICATES all_object_targets)
    set(${out_var} "${all_object_targets}" PARENT_SCOPE)
    return()
  endif()

  if(target_type STREQUAL "${MAGE_LIBRARY_TARGET_TYPE}")
    set(all_object_targets)
    mage_get_target_metadata(direct_deps "${target_name}" MAGE_DEPENDS)

    foreach(direct_dep IN LISTS direct_deps)
      mage_collect_object_targets("${direct_dep}" dep_object_targets)
      list(APPEND all_object_targets ${dep_object_targets})
    endforeach()

    list(REMOVE_DUPLICATES all_object_targets)
    set(${out_var} "${all_object_targets}" PARENT_SCOPE)
    return()
  endif()

  message(FATAL_ERROR
    "target '${target_name}' is not a Mage object/library target and "
    "cannot appear in DEPENDS")
endfunction()

function(mage_expand_object_targets out_var)
  set(all_object_targets)
  foreach(object_target IN LISTS ARGN)
    list(APPEND all_object_targets $<TARGET_OBJECTS:${object_target}>)
  endforeach()
  set(${out_var} "${all_object_targets}" PARENT_SCOPE)
endfunction()

function(mage_validate_test_dep_targets owner_target)
  foreach(dep_target IN LISTS ARGN)
    if(TARGET "${dep_target}")
      continue()
    endif()

    mage_get_declared_target_kinds(declared_target_kinds "${dep_target}")
    if(declared_target_kinds)
      if(MAGE_TARGET_IS_GPU)
        set(current_target_kind "GPU")
      else()
        set(current_target_kind "HOST")
      endif()

      list(JOIN declared_target_kinds ", " declared_target_kinds_str)

      message(FATAL_ERROR
        "${owner_target} depends on '${dep_target}', but '${dep_target}' "
        "is not available in the current ${current_target_kind} leaf. "
        "It was declared with TARGET_KINDS ${declared_target_kinds_str}")
    endif()

    message(FATAL_ERROR
      "${owner_target} depends on unknown target '${dep_target}'. "
      "Test dependencies must be defined before use")
  endforeach()
endfunction()

# ------------------------------------------------------------------------------
# Library target construction
# ------------------------------------------------------------------------------

# Defines a Mage object library for the current leaf.
#
# DEPENDS records structural Mage dependencies that are consumed later when a
# Mage library collects its transitive object closure.
function(mage_add_object_library target_name)
  cmake_parse_arguments(MAGE_OBJECT_LIBRARY
    "NO_COMMON_COMPILE_OPTIONS"
    ""
    "SRCS;TARGET_KINDS;DEPENDS;COMPILE_OPTIONS;LINK_LIBRARIES"
    ${ARGN})

  if(MAGE_OBJECT_LIBRARY_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_add_object_library(${target_name}) received unexpected arguments: "
      "${MAGE_OBJECT_LIBRARY_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_OBJECT_LIBRARY_SRCS)
    message(FATAL_ERROR
      "mage_add_object_library(${target_name}) requires SRCS")
  endif()

  mage_register_declared_target_kinds(
    ${target_name} TARGET_KINDS ${MAGE_OBJECT_LIBRARY_TARGET_KINDS})

  mage_target_matches_current_leaf(build_in_current_leaf
    TARGET_KINDS ${MAGE_OBJECT_LIBRARY_TARGET_KINDS})
  if(NOT build_in_current_leaf)
    return()
  endif()

  mage_validate_library_dep_targets(
    ${target_name} ${MAGE_OBJECT_LIBRARY_DEPENDS})

  if(MAGE_OBJECT_LIBRARY_NO_COMMON_COMPILE_OPTIONS)
    mage_resolve_common_compile_options(compile_options
      NO_COMMON_COMPILE_OPTIONS
      COMPILE_OPTIONS ${MAGE_OBJECT_LIBRARY_COMPILE_OPTIONS})
  else()
    mage_resolve_common_compile_options(compile_options
      COMPILE_OPTIONS ${MAGE_OBJECT_LIBRARY_COMPILE_OPTIONS})
  endif()

  add_library(${target_name} OBJECT ${MAGE_OBJECT_LIBRARY_SRCS})
  target_include_directories(${target_name}
    PRIVATE
      "${PROJECT_SOURCE_DIR}/include")

  if(compile_options)
    target_compile_options(${target_name}
      PRIVATE
        ${compile_options})
  endif()

  if(MAGE_OBJECT_LIBRARY_LINK_LIBRARIES)
    target_link_libraries(${target_name}
      PRIVATE
        ${MAGE_OBJECT_LIBRARY_LINK_LIBRARIES})
  endif()

  set_target_properties(${target_name} PROPERTIES
    MAGE_TARGET_TYPE "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}"
    MAGE_DEPENDS "${MAGE_OBJECT_LIBRARY_DEPENDS}"
    MAGE_LINK_LIBRARIES "${MAGE_OBJECT_LIBRARY_LINK_LIBRARIES}")
endfunction()

# Defines a Mage static library from local sources and/or structurally pure
# Mage library dependencies.
#
# When requested, GPU leaves can also emit a bitcode artifact for the final
# library.
function(mage_add_library target_name)
  set(mage_library_options
    GENERATE_GPU_BITCODE
    NO_COMMON_COMPILE_OPTIONS
    NO_COMMON_BITCODE_LINK_OPTIONS)

  set(mage_library_one_value_args)

  set(mage_library_multi_value_args
    SRCS
    TARGET_KINDS
    DEPENDS
    COMPILE_OPTIONS
    LINK_LIBRARIES
    BITCODE_LINK_OPTIONS)

  cmake_parse_arguments(
    MAGE_LIBRARY
    "${mage_library_options}"
    "${mage_library_one_value_args}"
    "${mage_library_multi_value_args}"
    ${ARGN})

  if(MAGE_LIBRARY_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_add_library(${target_name}) received unexpected arguments: "
      "${MAGE_LIBRARY_UNPARSED_ARGUMENTS}")
  endif()

  if(MAGE_LIBRARY_BITCODE_LINK_OPTIONS AND
     NOT MAGE_LIBRARY_GENERATE_GPU_BITCODE)
    message(FATAL_ERROR
      "mage_add_library(${target_name}): BITCODE_LINK_OPTIONS requires "
      "GENERATE_GPU_BITCODE")
  endif()

  if(MAGE_LIBRARY_NO_COMMON_BITCODE_LINK_OPTIONS AND
     NOT MAGE_LIBRARY_GENERATE_GPU_BITCODE)
    message(FATAL_ERROR
      "mage_add_library(${target_name}): NO_COMMON_BITCODE_LINK_OPTIONS "
      "requires GENERATE_GPU_BITCODE")
  endif()

  # GPU bitcode generation is only supported for structurally pure Mage
  # libraries, i.e. the final library must not use LINK_LIBRARIES and every
  # target reachable through DEPENDS must also be structurally pure.
  if(MAGE_LIBRARY_GENERATE_GPU_BITCODE)
    mage_target_supports_gpu(target_supports_gpu
      TARGET_KINDS ${MAGE_LIBRARY_TARGET_KINDS})
    if(NOT target_supports_gpu)
      message(FATAL_ERROR
        "mage_add_library(${target_name}): GENERATE_GPU_BITCODE requires the "
        "target to support GPU builds")
    endif()

    if(MAGE_LIBRARY_LINK_LIBRARIES)
      message(FATAL_ERROR
        "mage_add_library(${target_name}): GENERATE_GPU_BITCODE cannot be used "
        "together with LINK_LIBRARIES on the final library")
    endif()
  endif()

  if(NOT MAGE_LIBRARY_SRCS AND NOT MAGE_LIBRARY_DEPENDS)
    message(FATAL_ERROR
      "mage_add_library(${target_name}) requires SRCS and/or DEPENDS")
  endif()

  mage_register_declared_target_kinds(
    ${target_name} TARGET_KINDS ${MAGE_LIBRARY_TARGET_KINDS})

  mage_target_matches_current_leaf(build_in_current_leaf
    TARGET_KINDS ${MAGE_LIBRARY_TARGET_KINDS})
  if(NOT build_in_current_leaf)
    return()
  endif()

  mage_validate_library_dep_targets(${target_name} ${MAGE_LIBRARY_DEPENDS})

  set(structural_deps ${MAGE_LIBRARY_DEPENDS})

  if(MAGE_LIBRARY_SRCS)
    set(objects_target "${target_name}.__objects")
    set(object_library_args
      SRCS ${MAGE_LIBRARY_SRCS}
      TARGET_KINDS ${MAGE_LIBRARY_TARGET_KINDS}
      COMPILE_OPTIONS ${MAGE_LIBRARY_COMPILE_OPTIONS}
      LINK_LIBRARIES ${MAGE_LIBRARY_LINK_LIBRARIES})
    if(MAGE_LIBRARY_NO_COMMON_COMPILE_OPTIONS)
      list(APPEND object_library_args NO_COMMON_COMPILE_OPTIONS)
    endif()
    mage_add_object_library(${objects_target} ${object_library_args})
    list(APPEND structural_deps ${objects_target})
  endif()

  set(all_object_targets)
  foreach(dep_target IN LISTS structural_deps)
    mage_collect_object_targets("${dep_target}" dep_object_targets)
    list(APPEND all_object_targets ${dep_object_targets})
  endforeach()
  list(REMOVE_DUPLICATES all_object_targets)

  if(NOT all_object_targets)
    message(FATAL_ERROR
      "mage_add_library(${target_name}) resolved to an empty object closure")
  endif()

  mage_expand_object_targets(expanded_objects ${all_object_targets})

  add_library(${target_name} STATIC ${expanded_objects})
  target_include_directories(${target_name}
    PUBLIC
      "${PROJECT_SOURCE_DIR}/include")

  if(MAGE_LIBRARY_LINK_LIBRARIES)
    target_link_libraries(${target_name}
      PUBLIC
        ${MAGE_LIBRARY_LINK_LIBRARIES})
  endif()

  set_target_properties(${target_name} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    MAGE_TARGET_TYPE "${MAGE_LIBRARY_TARGET_TYPE}"
    MAGE_DEPENDS "${structural_deps}"
    MAGE_LINK_LIBRARIES "${MAGE_LIBRARY_LINK_LIBRARIES}")

  mage_register_archive_target("${target_name}")

  if(MAGE_TARGET_IS_GPU AND MAGE_LIBRARY_GENERATE_GPU_BITCODE)
    if(MAGE_LIBRARY_NO_COMMON_BITCODE_LINK_OPTIONS)
      mage_resolve_common_bitcode_link_options(bitcode_link_options
        NO_COMMON_BITCODE_LINK_OPTIONS
        BITCODE_LINK_OPTIONS ${MAGE_LIBRARY_BITCODE_LINK_OPTIONS})
    else()
      mage_resolve_common_bitcode_link_options(bitcode_link_options
        BITCODE_LINK_OPTIONS ${MAGE_LIBRARY_BITCODE_LINK_OPTIONS})
    endif()

    add_executable(${target_name}Bitcode ${expanded_objects})

    if(bitcode_link_options)
      target_link_options(${target_name}Bitcode
        PRIVATE
          ${bitcode_link_options})
    endif()

    set_target_properties(${target_name}Bitcode PROPERTIES
      OUTPUT_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}${target_name}"
      SUFFIX ".bc"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

    mage_register_bitcode_target("${target_name}Bitcode")
  endif()
endfunction()

# ------------------------------------------------------------------------------
# Unit test target construction
# ------------------------------------------------------------------------------

# Defines a Mage unit test executable for the current leaf.
#
# DEPENDS adds build-order dependencies only. It does not link libraries
# into the test executable. Libraries whose symbols are used by the test
# must be passed through LINK_LIBRARIES.
#
# On GPU leaves, tests are executed through MAGE_GPU_LOADER.
function(mage_add_unittest target_name)
  set(mage_unittest_options
    NO_COMMON_COMPILE_OPTIONS
    NO_COMMON_LINK_OPTIONS
    NO_COMMON_GPU_LOADER_LINK_OPTIONS)

  set(mage_unittest_one_value_args)

  set(mage_unittest_multi_value_args
    SRCS
    TARGET_KINDS
    DEPENDS
    COMPILE_OPTIONS
    LINK_OPTIONS
    LINK_LIBRARIES
    GPU_LOADER_LINK_OPTIONS)

  cmake_parse_arguments(
    MAGE_UNITTEST
    "${mage_unittest_options}"
    "${mage_unittest_one_value_args}"
    "${mage_unittest_multi_value_args}"
    ${ARGN})

  if(MAGE_UNITTEST_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mage_add_unittest(${target_name}) received unexpected arguments: "
      "${MAGE_UNITTEST_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_UNITTEST_SRCS)
    message(FATAL_ERROR "mage_add_unittest(${target_name}) requires SRCS")
  endif()

  mage_target_matches_current_leaf(build_in_current_leaf
    TARGET_KINDS ${MAGE_UNITTEST_TARGET_KINDS})
  if(NOT build_in_current_leaf)
    return()
  endif()

  mage_validate_test_dep_targets(${target_name} ${MAGE_UNITTEST_DEPENDS})

  if(MAGE_UNITTEST_NO_COMMON_COMPILE_OPTIONS)
    mage_resolve_common_compile_options(compile_options
      NO_COMMON_COMPILE_OPTIONS
      COMPILE_OPTIONS ${MAGE_UNITTEST_COMPILE_OPTIONS})
  else()
    mage_resolve_common_compile_options(compile_options
      COMPILE_OPTIONS ${MAGE_UNITTEST_COMPILE_OPTIONS})
  endif()

  if(MAGE_UNITTEST_NO_COMMON_LINK_OPTIONS)
    mage_resolve_common_link_options(link_options
      NO_COMMON_LINK_OPTIONS
      LINK_OPTIONS ${MAGE_UNITTEST_LINK_OPTIONS})
  else()
    mage_resolve_common_link_options(link_options
      LINK_OPTIONS ${MAGE_UNITTEST_LINK_OPTIONS})
  endif()

  if(MAGE_TARGET_IS_GPU)
    if(MAGE_GPU_LOADER STREQUAL "")
      message(FATAL_ERROR "GPU unit tests require MAGE_GPU_LOADER")
    endif()

    if(MAGE_UNITTEST_NO_COMMON_GPU_LOADER_LINK_OPTIONS)
      mage_resolve_common_gpu_loader_link_options(gpu_loader_link_options
        NO_COMMON_GPU_LOADER_LINK_OPTIONS
        GPU_LOADER_LINK_OPTIONS ${MAGE_UNITTEST_GPU_LOADER_LINK_OPTIONS})
    else()
      mage_resolve_common_gpu_loader_link_options(gpu_loader_link_options
        GPU_LOADER_LINK_OPTIONS ${MAGE_UNITTEST_GPU_LOADER_LINK_OPTIONS})
    endif()

    list(APPEND link_options ${gpu_loader_link_options})
  endif()

  add_executable(${target_name} ${MAGE_UNITTEST_SRCS})
  target_include_directories(${target_name}
    PRIVATE
      "${PROJECT_SOURCE_DIR}/include")

  if(compile_options)
    target_compile_options(${target_name}
      PRIVATE
        ${compile_options})
  endif()

  if(link_options)
    target_link_options(${target_name}
      PRIVATE
        ${link_options})
  endif()

  if(MAGE_UNITTEST_LINK_LIBRARIES)
    target_link_libraries(${target_name}
      PRIVATE
        ${MAGE_UNITTEST_LINK_LIBRARIES})
  endif()

  if(MAGE_UNITTEST_DEPENDS)
    add_dependencies(${target_name} ${MAGE_UNITTEST_DEPENDS})
  endif()

  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/unittests")

  add_dependencies(mage-unittests-build ${target_name})

  if(MAGE_TARGET_IS_GPU)
    set(mage_gpu_loader_args)
    if(NOT MAGE_GPU_LOADER_ARGS STREQUAL "")
      separate_arguments(mage_gpu_loader_args NATIVE_COMMAND
        "${MAGE_GPU_LOADER_ARGS}")
    endif()

    add_test(
      NAME ${target_name}
      COMMAND
        "${MAGE_GPU_LOADER}"
        ${mage_gpu_loader_args}
        $<TARGET_FILE:${target_name}>)
  else()
    add_test(
      NAME ${target_name}
      COMMAND $<TARGET_FILE:${target_name}>)
  endif()

  set_tests_properties(${target_name} PROPERTIES
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/unittests")
endfunction()
