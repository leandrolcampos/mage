//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares the backend abstraction used by device contexts.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_LIB_OFFLOAD_BACKEND_HPP
#define MAGE_LIB_OFFLOAD_BACKEND_HPP

#include "mage/Offload/Context.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <memory>
#include <mutex>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

namespace mage {
namespace detail {

class DeviceIdentity final {};

class [[nodiscard]] DeviceState {
public:
  virtual ~DeviceState() noexcept;

  DeviceState(const DeviceState &) = delete;
  DeviceState &operator=(const DeviceState &) = delete;
  DeviceState(DeviceState &&) = delete;
  DeviceState &operator=(DeviceState &&) = delete;

  [[nodiscard]] DeviceAPI getAPI() const noexcept;
  [[nodiscard]] int getID() const noexcept;
  [[nodiscard]] llvm::StringRef getName() const noexcept;
  [[nodiscard]] llvm::StringRef getArchitecture() const noexcept;
  [[nodiscard]] std::shared_ptr<const DeviceIdentity>
  getIdentity() const noexcept;

protected:
  DeviceState(DeviceAPI API, int DeviceID, std::string Name,
              std::string Architecture);

private:
  DeviceAPI API;
  int DeviceID;
  std::string Name;
  std::string Architecture;
  std::shared_ptr<const DeviceIdentity> Identity;
};

class [[nodiscard]] StreamState {
public:
  virtual ~StreamState() noexcept;

  StreamState(const StreamState &) = delete;
  StreamState &operator=(const StreamState &) = delete;
  StreamState(StreamState &&) = delete;
  StreamState &operator=(StreamState &&) = delete;

  virtual llvm::Error synchronize() = 0;
  virtual llvm::Expected<bool> hasPendingWork() const = 0;

protected:
  StreamState() noexcept = default;
};

class DeviceModuleStorage;

class [[nodiscard]] DeviceContextImpl {
public:
  virtual ~DeviceContextImpl() noexcept;

  DeviceContextImpl(const DeviceContextImpl &) = delete;
  DeviceContextImpl &operator=(const DeviceContextImpl &) = delete;
  DeviceContextImpl(DeviceContextImpl &&) = delete;
  DeviceContextImpl &operator=(DeviceContextImpl &&) = delete;

  [[nodiscard]] virtual DeviceAPI getAPI() const noexcept = 0;
  [[nodiscard]] virtual int getID() const noexcept = 0;
  [[nodiscard]] virtual llvm::StringRef getName() const = 0;
  [[nodiscard]] virtual llvm::StringRef getArchitecture() const = 0;
  virtual llvm::Expected<std::pair<size_t, size_t>> getMemoryInfo() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const DeviceIdentity>
  getDeviceIdentity() const noexcept = 0;

  virtual llvm::Expected<std::shared_ptr<HostBufferStorage>>
  createHostBufferStorage(size_t SizeInBytes) = 0;
  virtual llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
  createBufferStorage(size_t SizeInBytes) = 0;
  virtual llvm::Error enqueueCopyToDeviceStorage(
      std::shared_ptr<detail::DeviceBufferStorage> Dst,
      std::shared_ptr<const detail::HostBufferStorage> Src,
      size_t SizeInBytes) = 0;
  virtual llvm::Error enqueueCopyToHostStorage(
      std::shared_ptr<detail::HostBufferStorage> Dst,
      std::shared_ptr<const detail::DeviceBufferStorage> Src,
      size_t SizeInBytes) = 0;
  virtual llvm::Expected<std::shared_ptr<detail::DeviceModuleStorage>>
  loadModuleStorage(llvm::StringRef ImagePath) = 0;

  virtual llvm::Error synchronize() = 0;
  virtual llvm::Expected<bool> hasPendingWork() const = 0;

protected:
  DeviceContextImpl();

  void retainPendingResource(std::shared_ptr<const void> Resource);
  void releasePendingResources() noexcept;

private:
  mutable std::mutex PendingResourcesMutex;
  std::vector<std::shared_ptr<const void>> PendingResources;
};

class [[nodiscard]] Backend {
public:
  virtual ~Backend() noexcept = default;

  Backend(const Backend &) = delete;
  Backend &operator=(const Backend &) = delete;
  Backend(Backend &&) = delete;
  Backend &operator=(Backend &&) = delete;

  [[nodiscard]] virtual DeviceAPI getAPI() const noexcept = 0;
  [[nodiscard]] int getAPIVersion() const noexcept;
  [[nodiscard]] int getDeviceCount() const noexcept;
  virtual llvm::Expected<std::unique_ptr<DeviceContextImpl>>
  createDeviceContextImpl(int DeviceID) = 0;

protected:
  Backend(int APIVersion, int DeviceCount) noexcept
      : APIVersion(APIVersion), DeviceCount(DeviceCount) {}

private:
  int APIVersion;
  int DeviceCount;
};

[[nodiscard]] bool isBackendEnabled(DeviceAPI API) noexcept;

llvm::Expected<Backend &> getBackend(DeviceAPI API);

} // namespace detail
} // namespace mage

#endif // MAGE_LIB_OFFLOAD_BACKEND_HPP
