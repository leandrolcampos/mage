add_custom_target(mage-unittests)

set(ctest_args
  --progress
  --output-on-failure
  --test-dir "${CMAKE_BINARY_DIR}")

if(MAGE_BUILD_IS_GPU)
  list(APPEND ctest_args --parallel "${MAGE_GPU_TEST_PARALLELISM}")
endif()

add_custom_target(check-mage
  COMMAND
    "${CMAKE_CTEST_COMMAND}" ${ctest_args}
  DEPENDS
    mage
    mage-unittests
  USES_TERMINAL)

add_subdirectory(Foo)
