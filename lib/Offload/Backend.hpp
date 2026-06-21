//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares the backend abstraction and related helpers.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_LIB_OFFLOAD_BACKEND_HPP
#define MAGE_LIB_OFFLOAD_BACKEND_HPP

#include "mage/Offload/DeviceContext.hpp"

#include "llvm/Support/Error.h"

namespace mage {
namespace detail {

class Backend {
public:
  virtual ~Backend() = default;

  Backend(const Backend &) = delete;
  Backend &operator=(const Backend &) = delete;
  Backend(Backend &&) = delete;
  Backend &operator=(Backend &&) = delete;

  [[nodiscard]] virtual DeviceAPI getAPI() const = 0;

  [[nodiscard]] llvm::Expected<int> getAPIVersion() const;
  [[nodiscard]] llvm::Expected<int> getDeviceCount() const;

protected:
  Backend(int APIVersion, int DeviceCount)
      : APIVersion(APIVersion), DeviceCount(DeviceCount) {}

private:
  int APIVersion;
  int DeviceCount;
};

[[nodiscard]] bool isBackendEnabled(DeviceAPI API);

[[nodiscard]] llvm::Expected<Backend &> getBackend(DeviceAPI API);

} // namespace detail
} // namespace mage

#endif // MAGE_LIB_OFFLOAD_BACKEND_HPP
