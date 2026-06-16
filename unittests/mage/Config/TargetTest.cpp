//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests compile-time target architecture detection.
///
//===----------------------------------------------------------------------===//

#include "mage/Config/Target.hpp"
#include "UnitTest/Test.hpp"

static_assert(MAGE_TARGET_ARCH_IS_AMDGPU == 0 ||
              MAGE_TARGET_ARCH_IS_AMDGPU == 1);
static_assert(MAGE_TARGET_ARCH_IS_NVPTX == 0 || MAGE_TARGET_ARCH_IS_NVPTX == 1);
static_assert(MAGE_TARGET_ARCH_IS_GPU ==
              (MAGE_TARGET_ARCH_IS_AMDGPU || MAGE_TARGET_ARCH_IS_NVPTX));

#if defined(__AMDGPU__)
static_assert(MAGE_TARGET_ARCH_IS_AMDGPU);
static_assert(!MAGE_TARGET_ARCH_IS_NVPTX);
#elif defined(__NVPTX__)
static_assert(!MAGE_TARGET_ARCH_IS_AMDGPU);
static_assert(MAGE_TARGET_ARCH_IS_NVPTX);
#else
static_assert(!MAGE_TARGET_ARCH_IS_AMDGPU);
static_assert(!MAGE_TARGET_ARCH_IS_NVPTX);
#endif

MAGE_TEST(TargetTest, CompileTimeChecks) {}
