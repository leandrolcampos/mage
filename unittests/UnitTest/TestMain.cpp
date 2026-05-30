//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines the entry point for Mage unit-test executables.
///
//===----------------------------------------------------------------------===//

#include "UnitTest/Test.hpp"

int main() { return mage::testing::Test::runTests(); }
