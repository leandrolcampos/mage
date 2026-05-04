# CMake module for validating and applying Mage user-facing build options.
#
# This module is included for its side effects. User-facing cache variables must
# be declared in the top-level CMakeLists.txt before this module is included.

include_guard(GLOBAL)

if(MAGE_FORCE_ASSERTIONS)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" cmake_build_type_uppercase)
  if(NOT cmake_build_type_uppercase STREQUAL "DEBUG")
    add_compile_options(-UNDEBUG)
  endif()
endif()
