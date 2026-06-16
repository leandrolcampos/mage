//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests floating-point type aliases.
///
//===----------------------------------------------------------------------===//

#include "mage/Support/FloatTypes.hpp"
#include "UnitTest/Test.hpp"

#include "mage/Support/TypeTraits.hpp"

using namespace mage;

static_assert(is_same_v<float16, _Float16>, "float16 aliases _Float16");

MAGE_TEST(FloatTypesTest, CompileTimeChecks) {}
