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

#include <memory>
#include <stddef.h>
#include <string>
#include <utility>

namespace mage {
namespace detail {

class DeviceContextImpl {
public:
  virtual ~DeviceContextImpl() noexcept = default;

  DeviceContextImpl(const DeviceContextImpl &) = delete;
  DeviceContextImpl &operator=(const DeviceContextImpl &) = delete;
  DeviceContextImpl(DeviceContextImpl &&) = delete;
  DeviceContextImpl &operator=(DeviceContextImpl &&) = delete;

  [[nodiscard]] virtual DeviceAPI getAPI() const noexcept = 0;
  [[nodiscard]] virtual int getID() const noexcept = 0;
  [[nodiscard]] virtual llvm::Expected<std::string> getName() const = 0;
  [[nodiscard]] virtual llvm::Expected<std::string> getArchitecture() const = 0;
  [[nodiscard]] virtual llvm::Expected<std::pair<size_t, size_t>>
  getMemoryInfo() const = 0;

  virtual llvm::Error synchronize() = 0;

protected:
  DeviceContextImpl() noexcept = default;
};

class Backend {
public:
  virtual ~Backend() noexcept = default;

  Backend(const Backend &) = delete;
  Backend &operator=(const Backend &) = delete;
  Backend(Backend &&) = delete;
  Backend &operator=(Backend &&) = delete;

  [[nodiscard]] virtual DeviceAPI getAPI() const noexcept = 0;
  [[nodiscard]] int getAPIVersion() const noexcept;
  [[nodiscard]] int getDeviceCount() const noexcept;
  [[nodiscard]] virtual llvm::Expected<std::unique_ptr<DeviceContextImpl>>
  createDeviceContextImpl(int DeviceID) = 0;

protected:
  Backend(int APIVersion, int DeviceCount) noexcept
      : APIVersion(APIVersion), DeviceCount(DeviceCount) {}

private:
  int APIVersion;
  int DeviceCount;
};

[[nodiscard]] bool isBackendEnabled(DeviceAPI API) noexcept;

[[nodiscard]] llvm::Expected<Backend &> getBackend(DeviceAPI API);

} // namespace detail
} // namespace mage

#endif // MAGE_LIB_OFFLOAD_BACKEND_HPP
