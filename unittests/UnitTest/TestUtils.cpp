//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides C++ runtime entry points required by Mage unit tests.
///
//===----------------------------------------------------------------------===//

#include <stddef.h>
#include <stdlib.h>

// Mage unit tests intentionally avoid linking against the C++ runtime library,
// but the compiler may emit references to a few runtime entry points. So this
// file provides definitions for those entry points in the unit-test framework.

// The new operators are not part of the unit-test framework. They are defined
// alongside the delete operators so the replacement set is complete.
void *operator new([[maybe_unused]] size_t Size, void *Ptr) { return Ptr; }

void *operator new(size_t Size) { return malloc(Size); }

// Calling a pure virtual function is always a runtime error. Trap if the entry
// point is ever reached.
extern "C" void __cxa_pure_virtual() // NOLINT(readability-identifier-naming)
{
  __builtin_trap();
}

// The framework registers tests as static objects and should not destroy them
// through global delete. The delete operators only satisfy references emitted
// for virtual destructors and deleting destructors; trap on accidental use.
void operator delete(void *) noexcept { __builtin_trap(); }

void operator delete(void *, size_t) noexcept { __builtin_trap(); }
