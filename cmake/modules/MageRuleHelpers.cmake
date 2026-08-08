# Internal helper functions shared by Mage rule modules.

include_guard(GLOBAL)

set(MAGE_BITCODE_LIBRARY_TARGET_TYPE "MAGE_BITCODE_LIBRARY")
set(MAGE_DEVICE_IMAGE_TARGET_TYPE "MAGE_DEVICE_IMAGE")
set(MAGE_LIBRARY_TARGET_TYPE "MAGE_LIBRARY")
set(MAGE_OBJECT_LIBRARY_TARGET_TYPE "MAGE_OBJECT_LIBRARY")

set(MAGE_SOURCE_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")

# ------------------------------------------------------------------------------
# Build kind helpers
# ------------------------------------------------------------------------------

# Normalizes BUILD_KINDS arguments. An empty list means all build kinds.
function(_mage_normalize_build_kinds out_var build_kinds_list)
  set(normalized_build_kinds)

  if(build_kinds_list)
    foreach(build_kind IN LISTS build_kinds_list)
      string(STRIP "${build_kind}" build_kind)
      string(TOUPPER "${build_kind}" build_kind)

      if(NOT build_kind STREQUAL "HOST" AND
        NOT build_kind STREQUAL "GPU")
        message(FATAL_ERROR
          "unsupported build kind '${build_kind}'; expected HOST and/or GPU")
      endif()

      list(APPEND normalized_build_kinds "${build_kind}")
    endforeach()
  else()
    list(APPEND normalized_build_kinds HOST GPU)
  endif()

  list(REMOVE_DUPLICATES normalized_build_kinds)
  set(${out_var} "${normalized_build_kinds}" PARENT_SCOPE)
endfunction()

function(_mage_set_target_build_kinds target_name build_kinds_list)
  _mage_normalize_build_kinds(build_kinds "${build_kinds_list}")

  set_property(GLOBAL PROPERTY
    "MAGE_BUILD_KINDS_FOR_${target_name}" "${build_kinds}")
endfunction()

function(_mage_build_kinds_include_current_build_kind
    out_var build_kinds_list)
  _mage_normalize_build_kinds(build_kinds "${build_kinds_list}")

  if(MAGE_BUILD_KIND IN_LIST build_kinds)
    set(${out_var} ON PARENT_SCOPE)
  else()
    set(${out_var} OFF PARENT_SCOPE)
  endif()
endfunction()

function(_mage_get_target_build_kinds out_var target_name)
  get_property(build_kinds GLOBAL PROPERTY
    "MAGE_BUILD_KINDS_FOR_${target_name}")

  if(NOT build_kinds)
    set(build_kinds)
  endif()

  set(${out_var} "${build_kinds}" PARENT_SCOPE)
endfunction()

function(_mage_require_deps_available_in_current_build target_name deps_list)
  foreach(dep_target IN LISTS deps_list)
    if(NOT TARGET "${dep_target}")
      message(FATAL_ERROR
        "${target_name} depends on unknown target '${dep_target}'")
    endif()

    _mage_get_target_build_kinds(dep_build_kinds "${dep_target}")
    if(NOT dep_build_kinds)
      message(FATAL_ERROR
        "${target_name} depends on '${dep_target}', but '${dep_target}' "
        "was not registered with Mage build kinds")
    endif()

    _mage_build_kinds_include_current_build_kind(
      dep_enabled "${dep_build_kinds}")
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

# Resolves the LINK_LIBRARIES mini-language used by Mage rules. Each *_ONLY
# marker controls whether following items are included until the next marker.
function(_mage_resolve_conditional_link_libraries
    out_var link_libraries_list)
  set(resolved_link_libraries)
  set(include_current_section ON)

  foreach(link_library IN LISTS link_libraries_list)
    if(link_library STREQUAL "HOST_ONLY")
      if(MAGE_BUILD_KIND STREQUAL "HOST")
        set(include_current_section ON)
      else()
        set(include_current_section OFF)
      endif()
      continue()
    elseif(link_library STREQUAL "AMDGPU_ONLY")
      if(MAGE_TARGET_ARCH_IS_AMDGPU)
        set(include_current_section ON)
      else()
        set(include_current_section OFF)
      endif()
      continue()
    elseif(link_library STREQUAL "NVPTX_ONLY")
      if(MAGE_TARGET_ARCH_IS_NVPTX)
        set(include_current_section ON)
      else()
        set(include_current_section OFF)
      endif()
      continue()
    endif()

    if(include_current_section)
      list(APPEND resolved_link_libraries "${link_library}")
    endif()
  endforeach()

  set(${out_var} "${resolved_link_libraries}" PARENT_SCOPE)
endfunction()

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
  cmake_parse_arguments(COMMON_COMPILE_OPTIONS
    "IS_TEST"
    ""
    ""
    ${ARGN})

  if(COMMON_COMPILE_OPTIONS_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "_mage_get_common_compile_options received unexpected arguments: "
      "${COMMON_COMPILE_OPTIONS_UNPARSED_ARGUMENTS}")
  endif()

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
    -fno-exceptions
    -fno-lax-vector-conversions
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-rtti)

  if(NOT COMMON_COMPILE_OPTIONS_IS_TEST)
    list(APPEND compile_options -Wglobal-constructors)
  endif()

  if(MAGE_BUILD_KIND STREQUAL "GPU")
    list(APPEND compile_options
      --target=${MAGE_TARGET_TRIPLE}
      -nogpulib
      -fvisibility=hidden
      -fconvergent-functions
      -flto
      -Wno-multi-gpu)

    if(MAGE_TARGET_ARCH_IS_AMDGPU)
      list(APPEND compile_options
        "SHELL:-Xclang -mcode-object-version=none")
    elseif(MAGE_TARGET_ARCH_IS_NVPTX)
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
    "NO_COMMON_COMPILE_OPTIONS;IS_TEST"
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
    set(common_compile_option_args)
    if(OPTION_RESOLUTION_IS_TEST)
      list(APPEND common_compile_option_args IS_TEST)
    endif()

    _mage_get_common_compile_options(
      common_compile_options ${common_compile_option_args})
    list(APPEND compile_options ${common_compile_options})
  endif()

  list(APPEND compile_options ${OPTION_RESOLUTION_COMPILE_OPTIONS})

  set(${out_var} "${compile_options}" PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------------------------
# Link option helpers
# ------------------------------------------------------------------------------

# Resolves the GPU architecture used by GPU link-option helpers.
function(_mage_get_resolved_gpu_target_architecture out_var)
  if(NOT "${MAGE_GPU_TARGET_ARCHITECTURE}" STREQUAL "")
    set(${out_var} "${MAGE_GPU_TARGET_ARCHITECTURE}" PARENT_SCOPE)
    return()
  endif()

  if(MAGE_TARGET_ARCH_IS_AMDGPU)
    message(FATAL_ERROR "No AMDGPU architecture was detected or provided")
  endif()

  if(MAGE_TARGET_ARCH_IS_NVPTX)
    message(FATAL_ERROR "No NVPTX architecture was detected or provided")
  endif()

  message(FATAL_ERROR
    "unsupported GPU target triple in "
    "_mage_get_resolved_gpu_target_architecture: ${MAGE_TARGET_TRIPLE}")
endfunction()

function(_mage_get_common_link_options out_var)
  cmake_parse_arguments(COMMON_LINK_OPTIONS
    "IS_TEST"
    ""
    ""
    ${ARGN})

  if(COMMON_LINK_OPTIONS_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "_mage_get_common_link_options received unexpected arguments: "
      "${COMMON_LINK_OPTIONS_UNPARSED_ARGUMENTS}")
  endif()

  set(link_options)

  if(MAGE_BUILD_KIND STREQUAL "GPU")
    list(APPEND link_options
      --target=${MAGE_TARGET_TRIPLE}
      -flto)

    # GPU tests need startup files so llvm-gpu-loader can execute them.
    if(COMMON_LINK_OPTIONS_IS_TEST)
      list(APPEND link_options -startfiles)
    endif()

    _mage_get_resolved_gpu_target_architecture(gpu_target_architecture)
    if(MAGE_TARGET_ARCH_IS_AMDGPU)
      list(APPEND link_options -mcpu=${gpu_target_architecture})
    elseif(MAGE_TARGET_ARCH_IS_NVPTX)
      list(APPEND link_options -march=${gpu_target_architecture})
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

  if(MAGE_BUILD_KIND STREQUAL "GPU")
    list(APPEND link_options --target=${MAGE_TARGET_TRIPLE})

    _mage_get_resolved_gpu_target_architecture(gpu_target_architecture)

    if(MAGE_TARGET_ARCH_IS_AMDGPU)
      list(APPEND link_options -mcpu=${gpu_target_architecture})
    elseif(MAGE_TARGET_ARCH_IS_NVPTX)
      list(APPEND link_options -march=${gpu_target_architecture})
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
    "NO_COMMON_LINK_OPTIONS;IS_TEST"
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
    set(common_link_option_args)
    if(OPTION_RESOLUTION_IS_TEST)
      list(APPEND common_link_option_args IS_TEST)
    endif()

    _mage_get_common_link_options(
      common_link_options ${common_link_option_args})
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
# Device image registry helpers
# ------------------------------------------------------------------------------

function(_mage_get_device_image_output_dir out_var output_subdir)
  if((NOT DEFINED MAGE_INTERNAL_DEVICE_IMAGE_DIR) OR
     (MAGE_INTERNAL_DEVICE_IMAGE_DIR STREQUAL ""))
    message(FATAL_ERROR
      "MAGE_INTERNAL_DEVICE_IMAGE_DIR must be set before defining device "
      "images")
  endif()

  if(output_subdir)
    if(IS_ABSOLUTE "${output_subdir}")
      message(FATAL_ERROR
        "device image OUTPUT_SUBDIR must be relative, got '${output_subdir}'")
    endif()

    set(output_dir "${MAGE_INTERNAL_DEVICE_IMAGE_DIR}/${output_subdir}")
  else()
    set(output_dir "${MAGE_INTERNAL_DEVICE_IMAGE_DIR}")
  endif()

  set(${out_var} "${output_dir}" PARENT_SCOPE)
endfunction()

function(_mage_register_device_image target_name metadata_id output_subdir)
  get_property(device_image_targets GLOBAL PROPERTY MAGE_DEVICE_IMAGE_TARGETS)

  if("${target_name}" IN_LIST device_image_targets)
    message(FATAL_ERROR
      "device image '${target_name}' has already been registered")
  endif()

  if(NOT "${metadata_id}" MATCHES "^[A-Z][A-Z0-9_]*$")
    message(FATAL_ERROR
      "device image metadata ID '${metadata_id}' is invalid; expected a value "
      "matching ^[A-Z][A-Z0-9_]*$")
  endif()

  set(metadata_id_property
    "MAGE_DEVICE_IMAGE_TARGET_FOR_METADATA_ID_${metadata_id}")
  get_property(metadata_id_registered GLOBAL PROPERTY
    "${metadata_id_property}" SET)

  if(metadata_id_registered)
    get_property(existing_target GLOBAL PROPERTY "${metadata_id_property}")
    message(FATAL_ERROR
      "device image metadata ID '${metadata_id}' is already used by "
      "'${existing_target}'; it cannot also be used by '${target_name}'")
  endif()

  _mage_get_device_image_output_dir(output_dir "${output_subdir}")
  file(MAKE_DIRECTORY "${output_dir}")

  set_property(GLOBAL APPEND PROPERTY
    MAGE_DEVICE_IMAGE_TARGETS "${target_name}")
  set_property(GLOBAL PROPERTY
    "MAGE_DEVICE_IMAGE_OUTPUT_DIR_FOR_${target_name}" "${output_dir}")
  set_property(GLOBAL PROPERTY
    "MAGE_DEVICE_IMAGE_FILE_PREFIX_FOR_${target_name}" "${target_name}")
  set_property(GLOBAL PROPERTY
    "MAGE_DEVICE_IMAGE_METADATA_ID_FOR_${target_name}" "${metadata_id}")
  set_property(GLOBAL PROPERTY
    "${metadata_id_property}" "${target_name}")
endfunction()

function(_mage_get_device_image_property out_var target_name property_name)
  set(global_property_name
    "MAGE_DEVICE_IMAGE_${property_name}_FOR_${target_name}")
  get_property(property_is_set GLOBAL PROPERTY "${global_property_name}" SET)

  if(NOT property_is_set)
    message(FATAL_ERROR
      "unknown Mage device image '${target_name}'")
  endif()

  get_property(property_value GLOBAL PROPERTY "${global_property_name}")

  set(${out_var} "${property_value}" PARENT_SCOPE)
endfunction()

function(_mage_get_registered_device_images out_var)
  get_property(device_image_targets GLOBAL PROPERTY MAGE_DEVICE_IMAGE_TARGETS)

  if(NOT device_image_targets)
    set(device_image_targets)
  endif()

  set(${out_var} "${device_image_targets}" PARENT_SCOPE)
endfunction()

function(_mage_register_device_image_host_consumer
    device_image_target host_consumer_target)
  get_property(host_consumer_targets GLOBAL PROPERTY
    "MAGE_DEVICE_IMAGE_HOST_CONSUMERS_FOR_${device_image_target}")

  if("${host_consumer_target}" IN_LIST host_consumer_targets)
    message(FATAL_ERROR
      "host consumer '${host_consumer_target}' has already been registered "
      "for device image '${device_image_target}'")
  endif()

  set_property(GLOBAL APPEND PROPERTY
    "MAGE_DEVICE_IMAGE_HOST_CONSUMERS_FOR_${device_image_target}"
    "${host_consumer_target}")
endfunction()

function(_mage_add_existing_gpu_build_device_image_dependencies
    device_image_target host_consumer_target)
  get_property(gpu_build_targets GLOBAL PROPERTY
    "MAGE_GPU_BUILD_DEVICE_IMAGE_TARGETS_FOR_${device_image_target}")

  foreach(gpu_build_target IN LISTS gpu_build_targets)
    if(TARGET "${gpu_build_target}")
      add_dependencies("${host_consumer_target}" "${gpu_build_target}")
    endif()
  endforeach()
endfunction()

function(_mage_add_device_images_to_host_consumer
    host_consumer_target device_image_targets)
  foreach(device_image_target IN LISTS device_image_targets)
    _mage_get_device_image_property(
      output_dir "${device_image_target}" OUTPUT_DIR)
    _mage_get_device_image_property(
      file_prefix "${device_image_target}" FILE_PREFIX)
    _mage_get_device_image_property(
      metadata_id "${device_image_target}" METADATA_ID)

    target_compile_definitions("${host_consumer_target}"
      PRIVATE
        "MAGE_DEVICE_IMAGE_${metadata_id}_DIR=\"${output_dir}\""
        "MAGE_DEVICE_IMAGE_${metadata_id}_FILE_PREFIX=\"${file_prefix}\"")

    if(TARGET "${device_image_target}")
      add_dependencies("${host_consumer_target}" "${device_image_target}")
    endif()

    _mage_register_device_image_host_consumer(
      "${device_image_target}" "${host_consumer_target}")

    # Cover the less common order where GPU image targets already exist before
    # this host consumer is declared.
    _mage_add_existing_gpu_build_device_image_dependencies(
      "${device_image_target}" "${host_consumer_target}")
  endforeach()
endfunction()

# ------------------------------------------------------------------------------
# Object file collection helpers
# ------------------------------------------------------------------------------

# Performs a DFS over MAGE_DEPS to find object libraries needed by a target.
# The scratch global properties track recursion state for cycle detection.
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

# Collects transitive object-library targets and resets the DFS scratch state.
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
