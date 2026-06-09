# Rules for defining Mage command-line tools.

include_guard(GLOBAL)

# Rule to add a Mage command-line tool.
#
# Mage tools are host executables. They are excluded from the default build and
# can be built individually or through the mage-tools target.
#
# Usage:
#   add_mage_tool(
#     <target name>
#     SRCS <list of source files>
#     [COMPILE_OPTIONS <list of compile options>]
#     [LINK_OPTIONS <list of link options>]
#     [LINK_LIBRARIES <list of linking libraries for this target>]
#     [NO_COMMON_COMPILE_OPTIONS]
#     [NO_COMMON_LINK_OPTIONS]
#   )
function(add_mage_tool target_name)
  cmake_parse_arguments(MAGE_TOOL
    "NO_COMMON_COMPILE_OPTIONS;NO_COMMON_LINK_OPTIONS"
    ""
    "SRCS;COMPILE_OPTIONS;LINK_OPTIONS;LINK_LIBRARIES"
    ${ARGN})

  if(MAGE_TOOL_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_mage_tool(${target_name}) received unexpected arguments: "
      "${MAGE_TOOL_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_TOOL_SRCS)
    message(FATAL_ERROR
      "add_mage_tool(${target_name}) requires SRCS")
  endif()

  if(MAGE_BUILD_IS_GPU)
    message(FATAL_ERROR
      "add_mage_tool(${target_name}) cannot be used in a GPU build")
  endif()

  if(NOT TARGET mage-tools)
    message(FATAL_ERROR
      "add_mage_tool(${target_name}) requires the mage-tools target")
  endif()

  set(compile_option_args
    COMPILE_OPTIONS ${MAGE_TOOL_COMPILE_OPTIONS})
  if(MAGE_TOOL_NO_COMMON_COMPILE_OPTIONS)
    list(APPEND compile_option_args NO_COMMON_COMPILE_OPTIONS)
  endif()

  _mage_resolve_common_compile_options(compile_options ${compile_option_args})

  _mage_get_build_definitions(compile_definitions)

  set(link_option_args
    LINK_OPTIONS ${MAGE_TOOL_LINK_OPTIONS})
  if(MAGE_TOOL_NO_COMMON_LINK_OPTIONS)
    list(APPEND link_option_args NO_COMMON_LINK_OPTIONS)
  endif()

  _mage_resolve_common_link_options(link_options ${link_option_args})

  add_executable(${target_name} EXCLUDE_FROM_ALL
    ${MAGE_TOOL_SRCS})

  target_include_directories(${target_name}
    PRIVATE
      "${MAGE_SOURCE_INCLUDE_DIR}")

  if(compile_options)
    target_compile_options(${target_name}
      PRIVATE
        ${compile_options})
  endif()

  if(compile_definitions)
    target_compile_definitions(${target_name}
      PRIVATE
        ${compile_definitions})
  endif()

  if(link_options)
    target_link_options(${target_name}
      PRIVATE
        ${link_options})
  endif()

  if(MAGE_TOOL_LINK_LIBRARIES)
    target_link_libraries(${target_name}
      PRIVATE
        ${MAGE_TOOL_LINK_LIBRARIES})
  endif()

  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

  add_dependencies(mage-tools ${target_name})
endfunction()
