//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines device functions used by offload execution tests.
///
//===----------------------------------------------------------------------===//

#include <gpuintrin.h>

extern "C" {

__gpu_kernel void executionTestDoNothing() {}

__gpu_kernel void executionTestScale(int *Output, unsigned int ElementCount,
                                     int Factor) {
  unsigned int ThreadID = __gpu_thread_id(0);
  if (ThreadID < ElementCount)
    Output[ThreadID] = ThreadID * Factor;
}

__gpu_kernel void executionTestAdd(const int *Input, int *Output,
                                   unsigned int ElementCount, int Addend) {
  unsigned int ThreadID = __gpu_thread_id(0);
  if (ThreadID < ElementCount)
    Output[ThreadID] = Input[ThreadID] + Addend;
}

} // extern "C"
