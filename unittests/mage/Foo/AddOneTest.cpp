#include "mage/Foo/AddOne.hpp"
#include "UnitTest/Test.hpp"

MAGE_TEST(AddOneTest, AddsOne) {
  MAGE_EXPECT_EQ(mage::addOne(1), 2);
}
