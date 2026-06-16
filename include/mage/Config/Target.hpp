//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides compile-time target architecture detection.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_CONFIG_TARGET_HPP
#define MAGE_CONFIG_TARGET_HPP

#if defined(__AMDGPU__)
#define MAGE_TARGET_ARCH_IS_AMDGPU 1
#else
#define MAGE_TARGET_ARCH_IS_AMDGPU 0
#endif

#if defined(__NVPTX__)
#define MAGE_TARGET_ARCH_IS_NVPTX 1
#else
#define MAGE_TARGET_ARCH_IS_NVPTX 0
#endif

#define MAGE_TARGET_ARCH_IS_GPU                                                \
  (MAGE_TARGET_ARCH_IS_AMDGPU || MAGE_TARGET_ARCH_IS_NVPTX)

#endif // MAGE_CONFIG_TARGET_HPP
