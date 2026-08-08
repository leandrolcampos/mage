# Rules for defining Mage device images.

include_guard(GLOBAL)

# Rule to add a Mage device image.
#
# Usage:
#   add_mage_device_image(
#     <target name>
#     METADATA_ID <C/C++ macro identifier fragment>
#     SRCS <list of source files>
#     [OUTPUT_SUBDIR <subdir under the device image root directory>]
#     [BUILD_KINDS <HOST|GPU>...]
#     [DEPENDS <list of add_mage_object_library targets>]
#     [COMPILE_OPTIONS <list of compile options>]
#     [LINK_OPTIONS <list of link options>]
#     [LINK_LIBRARIES <list of linking libraries for this target>
#                     [HOST_ONLY|AMDGPU_ONLY|NVPTX_ONLY <items>...]
#                     [PUBLIC|PRIVATE|INTERFACE <items>...]]
#     [NO_COMMON_COMPILE_OPTIONS]
#     [NO_COMMON_LINK_OPTIONS]
#   )
#
# COMPILE_OPTIONS applies to SRCS. Sources from DEPENDS are compiled with
# the options of their object libraries.
#
# METADATA_ID must match [A-Z][A-Z0-9_]* and is used to define
# MAGE_DEVICE_IMAGE_<METADATA_ID>_DIR and
# MAGE_DEVICE_IMAGE_<METADATA_ID>_FILE_PREFIX for host consumers.
function(add_mage_device_image target_name)
  cmake_parse_arguments(MAGE_DEVICE_IMAGE
    "NO_COMMON_COMPILE_OPTIONS;NO_COMMON_LINK_OPTIONS"
    "METADATA_ID;OUTPUT_SUBDIR"
    "SRCS;BUILD_KINDS;DEPENDS;COMPILE_OPTIONS;LINK_OPTIONS;LINK_LIBRARIES"
    ${ARGN})

  if(MAGE_DEVICE_IMAGE_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_mage_device_image(${target_name}) received unexpected arguments: "
      "${MAGE_DEVICE_IMAGE_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_DEVICE_IMAGE_SRCS AND NOT MAGE_DEVICE_IMAGE_DEPENDS)
    message(FATAL_ERROR
      "add_mage_device_image(${target_name}) requires SRCS and/or DEPENDS")
  endif()

  if((NOT DEFINED MAGE_DEVICE_IMAGE_METADATA_ID) OR
     (MAGE_DEVICE_IMAGE_METADATA_ID STREQUAL ""))
    message(FATAL_ERROR
      "add_mage_device_image(${target_name}) requires METADATA_ID")
  endif()

  _mage_set_target_build_kinds(
    "${target_name}" "${MAGE_DEVICE_IMAGE_BUILD_KINDS}")
  _mage_register_device_image(
    "${target_name}"
    "${MAGE_DEVICE_IMAGE_METADATA_ID}"
    "${MAGE_DEVICE_IMAGE_OUTPUT_SUBDIR}")

  _mage_build_kinds_include_current_build_kind(
    device_image_enabled "${MAGE_DEVICE_IMAGE_BUILD_KINDS}")
  if(NOT device_image_enabled)
    return()
  endif()

  _mage_require_deps_available_in_current_build(
    "${target_name}" "${MAGE_DEVICE_IMAGE_DEPENDS}")

  set(allowed_target_types "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}")
  _mage_require_deps_have_allowed_target_types(
    "${target_name}"
    "${allowed_target_types}"
    "${MAGE_DEVICE_IMAGE_DEPENDS}")

  set(compile_option_args
    COMPILE_OPTIONS ${MAGE_DEVICE_IMAGE_COMPILE_OPTIONS})
  if(MAGE_DEVICE_IMAGE_NO_COMMON_COMPILE_OPTIONS)
    list(APPEND compile_option_args NO_COMMON_COMPILE_OPTIONS)
  endif()

  _mage_resolve_common_compile_options(compile_options ${compile_option_args})

  set(link_option_args
    LINK_OPTIONS ${MAGE_DEVICE_IMAGE_LINK_OPTIONS})
  if(MAGE_DEVICE_IMAGE_NO_COMMON_LINK_OPTIONS)
    list(APPEND link_option_args NO_COMMON_LINK_OPTIONS)
  endif()

  _mage_resolve_common_link_options(link_options ${link_option_args})

  _mage_resolve_conditional_link_libraries(
    resolved_link_libraries "${MAGE_DEVICE_IMAGE_LINK_LIBRARIES}")

  _mage_get_all_object_files_from_deps(
    all_object_files "${MAGE_DEVICE_IMAGE_DEPENDS}")

  if(MAGE_BUILD_KIND STREQUAL "HOST")
    add_library(${target_name} MODULE EXCLUDE_FROM_ALL
      ${MAGE_DEVICE_IMAGE_SRCS}
      ${all_object_files})

    set(output_directory_property LIBRARY_OUTPUT_DIRECTORY)
  else()
    add_executable(${target_name} EXCLUDE_FROM_ALL
      ${MAGE_DEVICE_IMAGE_SRCS}
      ${all_object_files})

    set(output_directory_property RUNTIME_OUTPUT_DIRECTORY)
  endif()

  target_include_directories(${target_name}
    PRIVATE
      "${MAGE_SOURCE_INCLUDE_DIR}")

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

  if(resolved_link_libraries)
    target_link_libraries(${target_name}
      PRIVATE
        ${resolved_link_libraries})
  endif()

  _mage_get_device_image_property(
    file_prefix "${target_name}" FILE_PREFIX)
  _mage_get_device_image_property(
    output_dir "${target_name}" OUTPUT_DIR)

  set_target_properties(${target_name} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${file_prefix}.${MAGE_TARGET_TRIPLE}"
    SUFFIX ".bin"
    "${output_directory_property}" "${output_dir}"
    MAGE_TARGET_TYPE "${MAGE_DEVICE_IMAGE_TARGET_TYPE}"
    MAGE_DEPS "${MAGE_DEVICE_IMAGE_DEPENDS}"
    MAGE_LINK_LIBRARIES "${resolved_link_libraries}")

  if(TARGET mage-device-images)
    add_dependencies(mage-device-images ${target_name})
  endif()
endfunction()

function(_mage_add_gpu_build_device_image_targets
    out_var gpu_target_triple gpu_build_binary_dir gpu_build_config_target)
  _mage_get_registered_device_images(device_image_targets)

  set(gpu_build_device_image_targets)

  foreach(device_image_target IN LISTS device_image_targets)
    _mage_get_target_build_kinds(
      device_image_build_kinds "${device_image_target}")

    if(NOT "GPU" IN_LIST device_image_build_kinds)
      continue()
    endif()

    set(gpu_build_device_image_target
      "mage-${gpu_target_triple}-device-image-${device_image_target}")

    add_custom_target("${gpu_build_device_image_target}"
      COMMAND
        "${CMAKE_COMMAND}" --build "${gpu_build_binary_dir}"
                          --target "${device_image_target}"
      DEPENDS
        "${gpu_build_config_target}"
      USES_TERMINAL)

    set_property(GLOBAL APPEND PROPERTY
      "MAGE_GPU_BUILD_DEVICE_IMAGE_TARGETS_FOR_${device_image_target}"
      "${gpu_build_device_image_target}")
    list(APPEND gpu_build_device_image_targets
      "${gpu_build_device_image_target}")

    # In the usual project order, host consumers are declared before GPU build
    # targets exist. Attach those consumers when creating each GPU image target.
    get_property(host_consumers GLOBAL PROPERTY
      "MAGE_DEVICE_IMAGE_HOST_CONSUMERS_FOR_${device_image_target}")

    foreach(host_consumer IN LISTS host_consumers)
      if(TARGET "${host_consumer}")
        add_dependencies("${host_consumer}" "${gpu_build_device_image_target}")
      endif()
    endforeach()

  endforeach()

  set(${out_var} "${gpu_build_device_image_targets}" PARENT_SCOPE)
endfunction()
