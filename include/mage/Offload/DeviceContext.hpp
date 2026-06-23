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

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "llvm/Support/Error.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace mage {

enum class DeviceAPI {
  CUDA,
  HIP,
};

[[nodiscard]] const char *toString(DeviceAPI API) noexcept;

/// Returns the number of devices available through \p API.
///
/// Returns 0 if Mage was built without runtime support for \p API.
[[nodiscard]] llvm::Expected<int> getDeviceCount(DeviceAPI API);

namespace detail {
class DeviceContextImpl;
} // namespace detail

/// Represents a single stream of execution on a particular GPU.
class DeviceContext {
public:
  ~DeviceContext() noexcept;

  DeviceContext(const DeviceContext &) = delete;
  DeviceContext &operator=(const DeviceContext &) = delete;
  DeviceContext(DeviceContext &&) noexcept;
  DeviceContext &operator=(DeviceContext &&) noexcept;

  static llvm::Expected<DeviceContext> create(DeviceAPI API, int DeviceID = 0);

  /// Returns the name of the API used to handle the underlying device.
  [[nodiscard]] DeviceAPI getAPI() const noexcept;

  /// Returns the ID associated with the underlying device.
  [[nodiscard]] int getID() const noexcept;

  /// Returns an identifier string for the underlying device.
  [[nodiscard]] llvm::Expected<std::string> getName() const;

  /// Returns the architecture name for the underlying device.
  [[nodiscard]] llvm::Expected<std::string> getArchitecture() const;

  /// Returns the free and total memory size for the underlying device.
  [[nodiscard]] llvm::Expected<std::pair<size_t, size_t>> getMemoryInfo() const;

  /// Blocks until all asynchronous calls on the underlying stream have
  /// completed.
  llvm::Error synchronize();

private:
  explicit DeviceContext(
      std::unique_ptr<detail::DeviceContextImpl> Impl) noexcept;

  std::unique_ptr<detail::DeviceContextImpl> Impl;
};

} // namespace mage

#endif // MAGE_OFFLOAD_DEVICE_CONTEXT_HPP
