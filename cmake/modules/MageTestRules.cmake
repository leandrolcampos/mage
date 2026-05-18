# Rules for defining Mage tests.

include_guard(GLOBAL)

# Rule to add a Mage unit test.
#
# Usage:
#   add_mage_unittest(
#     <target name>
#     SRCS <list of source files>
#     [BUILDS <HOST|GPU>...]
#     [DEPENDS <list of add_mage_object_library targets>]
#     [COMPILE_OPTIONS <list of compile options>]
#     [LINK_OPTIONS <list of link options>]
#     [LINK_LIBRARIES <list of linking libraries for this target>]
#     [NO_COMMON_COMPILE_OPTIONS]
#     [NO_COMMON_LINK_OPTIONS]
#   )
#
# COMPILE_OPTIONS applies to SRCS. Sources from DEPENDS are compiled with
# the options of their object libraries.
function(add_mage_unittest target_name)
  cmake_parse_arguments(MAGE_UNITTEST
    "NO_COMMON_COMPILE_OPTIONS;NO_COMMON_LINK_OPTIONS"
    ""
    "SRCS;BUILDS;DEPENDS;COMPILE_OPTIONS;LINK_OPTIONS;LINK_LIBRARIES"
    ${ARGN})

  if(MAGE_UNITTEST_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_mage_unittest(${target_name}) received unexpected arguments: "
      "${MAGE_UNITTEST_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT MAGE_UNITTEST_SRCS AND NOT MAGE_UNITTEST_DEPENDS)
    message(FATAL_ERROR
      "add_mage_unittest(${target_name}) requires SRCS and/or DEPENDS")
  endif()

  _mage_builds_include_current_build(unittest_enabled "${MAGE_UNITTEST_BUILDS}")
  if(NOT unittest_enabled)
    return()
  endif()

  _mage_require_deps_in_current_build(
    "${target_name}" "${MAGE_UNITTEST_DEPENDS}")

  set(allowed_target_types "${MAGE_OBJECT_LIBRARY_TARGET_TYPE}")
  _mage_require_deps_have_allowed_target_types(
    "${target_name}"
    "${allowed_target_types}"
    "${MAGE_UNITTEST_DEPENDS}")

  set(compile_option_args
    COMPILE_OPTIONS ${MAGE_UNITTEST_COMPILE_OPTIONS})
  if(MAGE_UNITTEST_NO_COMMON_COMPILE_OPTIONS)
    list(APPEND compile_option_args NO_COMMON_COMPILE_OPTIONS)
  endif()

  _mage_resolve_common_compile_options(compile_options ${compile_option_args})

  set(link_option_args
    LINK_OPTIONS ${MAGE_UNITTEST_LINK_OPTIONS})
  if(MAGE_UNITTEST_NO_COMMON_LINK_OPTIONS)
    list(APPEND link_option_args NO_COMMON_LINK_OPTIONS)
  endif()

  _mage_resolve_common_link_options(link_options ${link_option_args})

  if(MAGE_BUILD_IS_GPU AND NOT MAGE_UNITTEST_NO_COMMON_LINK_OPTIONS)
    list(APPEND link_options -stdlib -startfiles)
  endif()

  _mage_get_all_object_files_from_deps(
    all_object_files "${MAGE_UNITTEST_DEPENDS}")

  add_executable(${target_name}
    ${MAGE_UNITTEST_SRCS}
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

  if(MAGE_UNITTEST_LINK_LIBRARIES)
    target_link_libraries(${target_name}
      PRIVATE
        ${MAGE_UNITTEST_LINK_LIBRARIES})
  endif()

  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

  if(NOT TARGET mage-tests-build)
    add_custom_target(mage-tests-build)
  endif()

  add_dependencies(mage-tests-build ${target_name})

  if(MAGE_BUILD_IS_GPU)
    separate_arguments(llvm_gpu_loader_args NATIVE_COMMAND
      "${MAGE_LLVM_GPU_LOADER_ARGS}")

    add_test(
      NAME ${target_name}
      COMMAND
        "${MAGE_LLVM_GPU_LOADER}"
        ${llvm_gpu_loader_args}
        $<TARGET_FILE:${target_name}>)
  else()
    add_test(
      NAME ${target_name}
      COMMAND $<TARGET_FILE:${target_name}>)
  endif()
endfunction()
