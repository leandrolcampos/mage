//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements DeviceContext and related offload APIs.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/Context.hpp"

#include "Backend.hpp"

#include "mage/Offload/Execution.hpp"
#include "mage/Offload/Module.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"

#include <assert.h>
#include <limits>
#include <memory>
#include <stddef.h>
#include <string>
#include <utility>

using namespace mage;

const char *mage::toString(DeviceAPI API) noexcept {
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

DeviceContext::~DeviceContext() noexcept = default;

DeviceContext::DeviceContext(DeviceContext &&) noexcept = default;

DeviceContext &DeviceContext::operator=(DeviceContext &&) noexcept = default;

llvm::Expected<DeviceContext> DeviceContext::create(DeviceAPI API,
                                                    int DeviceID) {
  auto BackendOrErr = detail::getBackend(API);
  if (!BackendOrErr)
    return BackendOrErr.takeError();

  detail::Backend &Backend = *BackendOrErr;

  auto DeviceCount = Backend.getDeviceCount();
  if (DeviceID < 0 || DeviceID >= DeviceCount)
    return llvm::createStringError("device ID %d is out of range for the %s "
                                   "API; available device count is %d",
                                   DeviceID, toString(API), DeviceCount);

  auto ImplOrErr = Backend.createDeviceContextImpl(DeviceID);
  if (!ImplOrErr)
    return ImplOrErr.takeError();

  return DeviceContext(std::move(*ImplOrErr));
}

DeviceAPI DeviceContext::getAPI() const noexcept {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getAPI();
}

int DeviceContext::getID() const noexcept {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getID();
}

std::string DeviceContext::getName() const {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getName().str();
}

std::string DeviceContext::getArchitecture() const {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getArchitecture().str();
}

llvm::Expected<std::pair<size_t, size_t>> DeviceContext::getMemoryInfo() const {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getMemoryInfo();
}

llvm::Expected<bool> DeviceContext::hasPendingWork() const {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->hasPendingWork();
}

llvm::Expected<DeviceModule>
DeviceContext::loadModule(llvm::StringRef ImagePath) {
  assert(Impl && "cannot use a moved-from DeviceContext");

  auto StorageOrErr = Impl->loadModuleStorage(ImagePath);
  if (!StorageOrErr)
    return StorageOrErr.takeError();

  return DeviceModule(std::move(*StorageOrErr), Impl->getDeviceIdentity());
}

llvm::Error DeviceContext::synchronize() {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->synchronize();
}

DeviceContext::DeviceContext(
    std::unique_ptr<detail::DeviceContextImpl> Impl) noexcept
    : Impl(std::move(Impl)) {
  assert(this->Impl && "DeviceContext requires an implementation");
}

bool DeviceContext::ownsDeviceIdentity(
    const std::shared_ptr<const detail::DeviceIdentity> &Identity)
    const noexcept {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getDeviceIdentity() == Identity;
}

llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
DeviceContext::createHostBufferStorage(size_t SizeInBytes) {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->createHostBufferStorage(SizeInBytes);
}

llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
DeviceContext::createBufferStorage(size_t SizeInBytes) {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->createBufferStorage(SizeInBytes);
}

llvm::Error DeviceContext::enqueueCopyToDeviceStorage(
    std::shared_ptr<detail::DeviceBufferStorage> Dst,
    std::shared_ptr<const detail::HostBufferStorage> Src, size_t SizeInBytes) {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->enqueueCopyToDeviceStorage(std::move(Dst), std::move(Src),
                                          SizeInBytes);
}

llvm::Error DeviceContext::enqueueCopyToHostStorage(
    std::shared_ptr<detail::HostBufferStorage> Dst,
    std::shared_ptr<const detail::DeviceBufferStorage> Src,
    size_t SizeInBytes) {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->enqueueCopyToHostStorage(std::move(Dst), std::move(Src),
                                        SizeInBytes);
}

static llvm::Error validateLaunchDim(Dim3 Dim, const char *Name) {
  if (Dim.X == 0 || Dim.Y == 0 || Dim.Z == 0)
    return llvm::createStringError(
        "cannot enqueue a launch with %s dimensions (%u, %u, %u): all "
        "dimensions must be non-zero",
        Name, static_cast<unsigned int>(Dim.X),
        static_cast<unsigned int>(Dim.Y), static_cast<unsigned int>(Dim.Z));

  return llvm::Error::success();
}

static llvm::Error validateLaunchConfig(const LaunchConfig &Config) {
  if (auto Err = validateLaunchDim(Config.GridDim, "grid"))
    return Err;

  if (auto Err = validateLaunchDim(Config.BlockDim, "block"))
    return Err;

  constexpr auto MaxDynamicSharedMemoryBytes =
      static_cast<size_t>(std::numeric_limits<unsigned int>::max());
  if (Config.DynamicSharedMemoryBytes > MaxDynamicSharedMemoryBytes)
    return llvm::createStringError(
        "cannot enqueue a launch with %zu bytes of dynamic shared memory; "
        "maximum representable size is %zu bytes",
        Config.DynamicSharedMemoryBytes, MaxDynamicSharedMemoryBytes);

  return llvm::Error::success();
}

llvm::Error DeviceContext::enqueueLaunchImpl(
    std::shared_ptr<detail::DeviceFunctionStorage> Function,
    const LaunchConfig &Config, llvm::MutableArrayRef<void *> ArgPtrs,
    llvm::ArrayRef<std::shared_ptr<const void>> PendingResources) {
  assert(Impl && "cannot use a moved-from DeviceContext");

  if (auto Err = validateLaunchConfig(Config))
    return Err;

  return Impl->enqueueLaunchImpl(std::move(Function), Config, ArgPtrs,
                                 PendingResources);
}
