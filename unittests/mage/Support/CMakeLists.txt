add_mage_unittest(
  BitTest
  SRCS
    BitTest.cpp
  BUILD_KINDS
    HOST
    GPU
)

add_mage_unittest(
  FloatTypesTest
  SRCS
    FloatTypesTest.cpp
  BUILD_KINDS
    HOST
    GPU
)

add_mage_unittest(
  ParallelTest
  SRCS
    ParallelTest.cpp
  BUILD_KINDS
    HOST
  LINK_LIBRARIES
    MageSupport
    Mage::LLVMSupport
)

add_mage_unittest(
  RangeTest
  SRCS
    RangeTest.cpp
  BUILD_KINDS
    HOST
    GPU
)

add_mage_unittest(
  SequentialTest
  SRCS
    SequentialTest.cpp
  BUILD_KINDS
    HOST
  LINK_LIBRARIES
    MageSupport
    Mage::LLVMSupport
)

add_mage_unittest(
  TypeTraitsTest
  SRCS
    TypeTraitsTest.cpp
  BUILD_KINDS
    HOST
    GPU
)
