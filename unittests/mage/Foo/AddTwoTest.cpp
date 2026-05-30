#include "mage/Foo/AddTwo.hpp"
#include "UnitTest/Test.hpp"

MAGE_TEST(AddTwoTest, AddsTwo) {
  MAGE_EXPECT_EQ(mage::addTwo(1), 3);
}
