//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests bit manipulation utilities.
///
//===----------------------------------------------------------------------===//

#include "mage/Support/Bit.hpp"
#include "UnitTest/Test.hpp"

#include "mage/Support/FloatTypes.hpp"

#include <stdint.h>

using namespace mage;

static_assert(bit_cast<uint32_t>(INT32_MIN) == 0x80000000u);
static_assert(bit_cast<int32_t>(uint32_t(0x80000000)) == INT32_MIN);
static_assert(bit_cast<uint32_t>(int32_t(-1)) == UINT32_MAX);
static_assert(bit_cast<int32_t>(UINT32_MAX) == -1);

static_assert(bit_cast<uint16_t>(float16(1.0)) == 0x3c00u);
static_assert(bit_cast<float16>(uint16_t(0x3c00)) == float16(1.0));

static_assert(bit_cast<uint32_t>(1.0f) == 0x3f800000u);
static_assert(bit_cast<float>(uint32_t(0x3f800000)) == 1.0f);

static_assert(bit_cast<uint64_t>(1.0) == 0x3ff0000000000000ull);
static_assert(bit_cast<double>(uint64_t(0x3ff0000000000000)) == 1.0);

MAGE_TEST(BitTest, CompileTimeChecks) {}
