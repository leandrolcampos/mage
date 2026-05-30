add_mage_unittest(
  AddOneTest
  SRCS
    AddOneTest.cpp
  LINK_LIBRARIES
    MageFoo
)

add_mage_unittest(
  AddTwoTest
  SRCS
    AddTwoTest.cpp
  DEPENDS
    MageFooAddTwo
)
