//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines device symbols used by offload module tests.
///
//===----------------------------------------------------------------------===//

#include <gpuintrin.h>

extern "C" {

[[gnu::visibility("default")]]
int ModuleTestGlobal = 42;

int moduleTestNotLaunchable(int Value) { return Value + ModuleTestGlobal; }

__gpu_kernel void moduleTestDoNothing() {}

__gpu_kernel void moduleTestUseRegisters(int *Output) {
  int ThreadID = __gpu_thread_id(0);
  int Value = ThreadID + 1;
  Value = Value * 3 + ThreadID;
  Output[ThreadID] = Value;
}

} // extern "C"
