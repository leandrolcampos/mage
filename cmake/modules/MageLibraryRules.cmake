# Rules for defining Mage libraries.

include_guard(GLOBAL)

# Rule to add a Mage object library.
#
# Usage:
#   add_mage_object_library(
#     <target name>
#     SRCS <list of source files>
#     [BUILDS <HOST|GPU>...]
#     [DEPENDS <list of add_mage_object_library targets>]
#     [COMPILE_OPTIONS <list of compile options>]
#     [NO_COMMON_COMPILE_OPTIONS]
#   )
function(add_mage_object_library target_name)
  cmake_parse_arguments(MAGE_OBJECT_LIBRARY
    "NO_COMMON_COMPILE_OPTIONS"
    ""
    "SRCS;BUILDS;DEPENDS;COMPILE_OPTIONS"
    ${ARGN})

  if(MAGE_OBJECT_LIBRARY_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_mage_object_library(${target_name}) received unexpected arguments: "
      "${MAGE_OBJECT_LIBRARY_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_OBJECT_LIBRARY_SRCS)
    message(FATAL_ERROR
      "add_mage_object_library(${target_name}) requires SRCS")
  endif()

  _mage_set_builds_for_target(
    "${target_name}" "${MAGE_OBJECT_LIBRARY_BUILDS}")

  _mage_builds_include_current_build(object_library_enabled
    "${MAGE_OBJECT_LIBRARY_BUILDS}")
  if(NOT object_library_enabled)
    return()
  endif()

  _mage_require_deps_in_current_build(
    "${target_name}" "${MAGE_OBJECT_LIBRARY_DEPENDS}")

  set(allowed_target_types "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}")
  _mage_require_deps_have_allowed_target_types(
    "${target_name}"
    "${allowed_target_types}"
    "${MAGE_OBJECT_LIBRARY_DEPENDS}")

  set(compile_option_args
    COMPILE_OPTIONS ${MAGE_OBJECT_LIBRARY_COMPILE_OPTIONS})
  if(MAGE_OBJECT_LIBRARY_NO_COMMON_COMPILE_OPTIONS)
    list(APPEND compile_option_args NO_COMMON_COMPILE_OPTIONS)
  endif()

  _mage_resolve_common_compile_options(compile_options ${compile_option_args})

  add_library(${target_name} OBJECT EXCLUDE_FROM_ALL
    ${MAGE_OBJECT_LIBRARY_SRCS})

  target_include_directories(${target_name}
    PRIVATE
      "${MAGE_SOURCE_INCLUDE_DIR}")

  if(compile_options)
    target_compile_options(${target_name}
      PRIVATE
        ${compile_options})
  endif()

  if(MAGE_OBJECT_LIBRARY_DEPENDS)
    # Propagate usage requirements from dependent object libraries.
    target_link_libraries(${target_name}
      PUBLIC
        ${MAGE_OBJECT_LIBRARY_DEPENDS})
  endif()

  set_target_properties(${target_name} PROPERTIES
    MAGE_TARGET_TYPE "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}"
    MAGE_DEPS "${MAGE_OBJECT_LIBRARY_DEPENDS}")
endfunction()

# Rule to add a Mage library.
#
# Usage:
#   add_mage_library(
#     <target name>
#     SRCS <list of source files>
#     [BUILDS <HOST|GPU>...]
#     [DEPENDS <list of add_mage_object_library targets>]
#     [COMPILE_OPTIONS <list of compile options>]
#     [LINK_LIBRARIES <list of linking libraries for this target>
#                     [PUBLIC|PRIVATE|INTERFACE <items>...]]
#     [NO_COMMON_COMPILE_OPTIONS]
#   )
#
# COMPILE_OPTIONS applies to SRCS. Sources from DEPENDS are compiled with
# the options of their object libraries.
function(add_mage_library target_name)
  cmake_parse_arguments(MAGE_LIBRARY
    "NO_COMMON_COMPILE_OPTIONS"
    ""
    "SRCS;BUILDS;DEPENDS;COMPILE_OPTIONS;LINK_LIBRARIES"
    ${ARGN})

  if(MAGE_LIBRARY_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_mage_library(${target_name}) received unexpected arguments: "
      "${MAGE_LIBRARY_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_LIBRARY_SRCS AND NOT MAGE_LIBRARY_DEPENDS)
    message(FATAL_ERROR
      "add_mage_library(${target_name}) requires SRCS and/or DEPENDS")
  endif()

  _mage_set_builds_for_target("${target_name}" "${MAGE_LIBRARY_BUILDS}")

  _mage_builds_include_current_build(library_enabled "${MAGE_LIBRARY_BUILDS}")
  if(NOT library_enabled)
    return()
  endif()

  _mage_require_deps_in_current_build(
    "${target_name}" "${MAGE_LIBRARY_DEPENDS}")

  set(allowed_target_types "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}")
  _mage_require_deps_have_allowed_target_types(
    "${target_name}"
    "${allowed_target_types}"
    "${MAGE_LIBRARY_DEPENDS}")

  set(compile_option_args
    COMPILE_OPTIONS ${MAGE_LIBRARY_COMPILE_OPTIONS})
  if(MAGE_LIBRARY_NO_COMMON_COMPILE_OPTIONS)
    list(APPEND compile_option_args NO_COMMON_COMPILE_OPTIONS)
  endif()

  _mage_resolve_common_compile_options(compile_options ${compile_option_args})

  _mage_get_all_object_files_from_deps(
    all_object_files "${MAGE_LIBRARY_DEPENDS}")

  add_library(${target_name} STATIC EXCLUDE_FROM_ALL
    ${MAGE_LIBRARY_SRCS}
    ${all_object_files})

  target_include_directories(${target_name}
    PUBLIC
      "${MAGE_SOURCE_INCLUDE_DIR}")

  if(compile_options)
    target_compile_options(${target_name}
      PRIVATE
        ${compile_options})
  endif()

  if(MAGE_LIBRARY_LINK_LIBRARIES)
    target_link_libraries(${target_name}
      PUBLIC
      ${MAGE_LIBRARY_LINK_LIBRARIES})
  endif()

  set_target_properties(${target_name} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    MAGE_TARGET_TYPE "${MAGE_LIBRARY_TARGET_TYPE}"
    MAGE_DEPS "${MAGE_LIBRARY_DEPENDS}"
    MAGE_LINK_LIBRARIES "${MAGE_LIBRARY_LINK_LIBRARIES}")

  add_dependencies(mage-archives ${target_name})
endfunction()

# Rule to add a Mage library and bundle it in a single LLVM-IR bitcode file.
#
# Usage:
#   add_mage_bitcode_library(
#     <target name>
#     SRCS <list of source files>
#     [BUILDS <HOST|GPU>...]
#     [DEPENDS <list of add_mage_object_library targets>]
#     [COMPILE_OPTIONS <list of compile options>]
#     [LINK_OPTIONS <list of link options>]
#     [NO_COMMON_COMPILE_OPTIONS]
#     [NO_COMMON_LINK_OPTIONS]
#   )
#
# COMPILE_OPTIONS applies to SRCS. Sources from DEPENDS are compiled with
# the options of their object libraries.
#
# In host builds, callers are responsible for providing bitcode-compatible
# inputs, such as objects compiled with -flto when needed.
function(add_mage_bitcode_library target_name)
  cmake_parse_arguments(MAGE_BITCODE_LIBRARY
    "NO_COMMON_COMPILE_OPTIONS;NO_COMMON_LINK_OPTIONS"
    ""
    "SRCS;BUILDS;DEPENDS;COMPILE_OPTIONS;LINK_OPTIONS"
    ${ARGN})

  if(MAGE_BITCODE_LIBRARY_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_mage_bitcode_library(${target_name}) received unexpected arguments: "
      "${MAGE_BITCODE_LIBRARY_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_BITCODE_LIBRARY_SRCS AND NOT MAGE_BITCODE_LIBRARY_DEPENDS)
    message(FATAL_ERROR
      "add_mage_bitcode_library(${target_name}) requires SRCS and/or DEPENDS")
  endif()

  _mage_set_builds_for_target(
    "${target_name}" "${MAGE_BITCODE_LIBRARY_BUILDS}")

  _mage_builds_include_current_build(
    bitcode_library_enabled "${MAGE_BITCODE_LIBRARY_BUILDS}")
  if(NOT bitcode_library_enabled)
    return()
  endif()

  _mage_require_deps_in_current_build(
    "${target_name}" "${MAGE_BITCODE_LIBRARY_DEPENDS}")

  set(allowed_target_types "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}")
  _mage_require_deps_have_allowed_target_types(
    "${target_name}"
    "${allowed_target_types}"
    "${MAGE_BITCODE_LIBRARY_DEPENDS}")

  set(compile_option_args
    COMPILE_OPTIONS ${MAGE_BITCODE_LIBRARY_COMPILE_OPTIONS})
  if(MAGE_BITCODE_LIBRARY_NO_COMMON_COMPILE_OPTIONS)
    list(APPEND compile_option_args NO_COMMON_COMPILE_OPTIONS)
  endif()

  _mage_resolve_common_compile_options(compile_options ${compile_option_args})

  set(link_option_args
    LINK_OPTIONS ${MAGE_BITCODE_LIBRARY_LINK_OPTIONS})
  if(MAGE_BITCODE_LIBRARY_NO_COMMON_LINK_OPTIONS)
    list(APPEND link_option_args NO_COMMON_LINK_OPTIONS)
  endif()

  _mage_resolve_common_bitcode_link_options(link_options ${link_option_args})

  _mage_get_all_object_files_from_deps(
    all_object_files "${MAGE_BITCODE_LIBRARY_DEPENDS}")

  add_executable(${target_name} EXCLUDE_FROM_ALL
    ${MAGE_BITCODE_LIBRARY_SRCS}
    ${all_object_files})

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

  set_target_properties(${target_name} PROPERTIES
    OUTPUT_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}${target_name}"
    SUFFIX ".bc"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    MAGE_TARGET_TYPE "${MAGE_BITCODE_LIBRARY_TARGET_TYPE}"
    MAGE_DEPS "${MAGE_BITCODE_LIBRARY_DEPENDS}")

  add_dependencies(mage-bitcode-libraries ${target_name})
endfunction()
