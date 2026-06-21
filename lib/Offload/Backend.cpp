//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements backend selection and shared backend helpers.
///
//===----------------------------------------------------------------------===//

#include "Backend.hpp"

#if MAGE_CUDA_BACKEND_ENABLED
#include "CUDABackend.hpp"
#endif

#if MAGE_HIP_BACKEND_ENABLED
#include "HIPBackend.hpp"
#endif

#include "mage/Offload/DeviceContext.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"

using namespace mage;

bool detail::isBackendEnabled(DeviceAPI API) {
  switch (API) {
  case DeviceAPI::CUDA:
    return MAGE_CUDA_BACKEND_ENABLED;
  case DeviceAPI::HIP:
    return MAGE_HIP_BACKEND_ENABLED;
  }
  // TODO: Use MAGE_UNREACHABLE once available.
  llvm_unreachable("unknown device API");
}

llvm::Expected<detail::Backend &> detail::getBackend(DeviceAPI API) {
#if MAGE_CUDA_BACKEND_ENABLED
  if (API == DeviceAPI::CUDA)
    return getCUDABackend();
#endif

#if MAGE_HIP_BACKEND_ENABLED
  if (API == DeviceAPI::HIP)
    return getHIPBackend();
#endif

  return llvm::createStringError("backend for the %s API is not enabled",
                                 toString(API));
}
