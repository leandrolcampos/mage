//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements DeviceContext and related device-query APIs.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/DeviceContext.hpp"

#include "Backend.hpp"

#include "llvm/Support/ErrorHandling.h"

const char *mage::toString(DeviceAPI API) {
  switch (API) {
  case DeviceAPI::CUDA:
    return "CUDA";
  case DeviceAPI::HIP:
    return "HIP";
  }
  // TODO: Use MAGE_UNREACHABLE once available.
  llvm_unreachable("unknown device API");
}

llvm::Expected<int> mage::getDeviceCount(DeviceAPI API) {
  if (!detail::isBackendEnabled(API))
    return 0;

  auto BackendOrErr = detail::getBackend(API);
  if (!BackendOrErr)
    return BackendOrErr.takeError();

  return (*BackendOrErr).getDeviceCount();
}
