//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements utilities to handle recoverable errors.
///
//===----------------------------------------------------------------------===//

#include "mage/Support/Error.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <utility>

using namespace mage;

void mage::consumeErrorWithDebugLogging(llvm::Error Err) noexcept {
#ifndef NDEBUG
  llvm::logAllUnhandledErrors(std::move(Err), llvm::errs());
#else
  llvm::consumeError(std::move(Err));
#endif
}
