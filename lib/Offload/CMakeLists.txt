set(offload_sources
  Backend.cpp
  Context.cpp
  Memory.cpp)

set(offload_link_libraries
  MageSupport
  Mage::LLVMSupport)

set(offload_include_directories
  ${CMAKE_CURRENT_SOURCE_DIR})

if(MAGE_CUDA_BACKEND_ENABLED)
  list(APPEND offload_sources
    CUDA/CUDABackend.cpp)
  list(APPEND offload_link_libraries
    Mage::CUDADriver)
  list(APPEND offload_include_directories
    ${CMAKE_CURRENT_SOURCE_DIR}/CUDA)
endif()

if(MAGE_HIP_BACKEND_ENABLED)
  list(APPEND offload_sources
    HIP/HIPBackend.cpp)
  list(APPEND offload_link_libraries
    Mage::HIPRuntime)
  list(APPEND offload_include_directories
    ${CMAKE_CURRENT_SOURCE_DIR}/HIP)
endif()

add_mage_library(
  MageOffload
  SRCS
    ${offload_sources}
  BUILD_KINDS
    HOST
  LINK_LIBRARIES
    ${offload_link_libraries}
)

if(TARGET MageOffload)
  target_include_directories(MageOffload
    PRIVATE
      ${offload_include_directories}
  )

  if(MAGE_CUDA_BACKEND_ENABLED)
    target_compile_definitions(MageOffload
      PRIVATE
        MAGE_CUDA_BACKEND_ENABLED=1)
  else()
    target_compile_definitions(MageOffload
      PRIVATE
        MAGE_CUDA_BACKEND_ENABLED=0)
  endif()

  if(MAGE_HIP_BACKEND_ENABLED)
    target_compile_definitions(MageOffload
      PRIVATE
        MAGE_HIP_BACKEND_ENABLED=1)
  else()
    target_compile_definitions(MageOffload
      PRIVATE
        MAGE_HIP_BACKEND_ENABLED=0)
  endif()
endif()
