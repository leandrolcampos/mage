//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests Mage floating-point type aliases.
///
//===----------------------------------------------------------------------===//

#include "mage/MathExtras/FloatTypes.hpp"
#include "UnitTest/Test.hpp"

static_assert(__is_same(mage::float16, _Float16), "float16 aliases _Float16");

MAGE_TEST(FloatTypesTest, CompileTimeChecks) {}
