//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares utilities to handle recoverable errors.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_SUPPORT_ERROR_HPP
#define MAGE_SUPPORT_ERROR_HPP

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "llvm/Support/Error.h"

namespace mage {

void consumeErrorWithDebugLogging(llvm::Error Err) noexcept;

} // namespace mage

#endif // MAGE_SUPPORT_ERROR_HPP
