//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares DeviceContext and related device-query APIs.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_OFFLOAD_DEVICE_CONTEXT_HPP
#define MAGE_OFFLOAD_DEVICE_CONTEXT_HPP

#include "llvm/Support/Error.h"

namespace mage {

using DeviceID = int;

enum class DeviceAPI {
  CUDA,
  HIP,
};

[[nodiscard]] const char *toString(DeviceAPI API);

/// Returns the number of devices available through \p API.
///
/// Returns 0 if Mage was built without runtime support for \p API.
[[nodiscard]] llvm::Expected<int> getDeviceCount(DeviceAPI API);

} // namespace mage

#endif // MAGE_OFFLOAD_DEVICE_CONTEXT_HPP
