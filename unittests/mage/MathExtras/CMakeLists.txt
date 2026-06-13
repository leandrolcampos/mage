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
  TypeTraitsTest
  SRCS
    TypeTraitsTest.cpp
  BUILD_KINDS
    HOST
    GPU
)
