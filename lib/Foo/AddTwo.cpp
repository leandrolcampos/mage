#include "mage/Foo/AddTwo.hpp"
#include "mage/Foo/AddOne.hpp"

int mage::addTwo(int Number) { return addOne(addOne(Number)); }
