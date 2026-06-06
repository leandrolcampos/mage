add_mage_unittest(
  BitTest
  SRCS
    BitTest.cpp
  BUILDS
    HOST
    GPU
)

add_mage_unittest(
  FloatTypesTest
  SRCS
    FloatTypesTest.cpp
  BUILDS
    HOST
    GPU
)

add_mage_unittest(
  TypeTraitsTest
  SRCS
    TypeTraitsTest.cpp
  BUILDS
    HOST
    GPU
)
