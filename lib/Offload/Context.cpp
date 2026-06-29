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

#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"

#include <assert.h>
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

DeviceContext::DeviceContext(
    std::unique_ptr<detail::DeviceContextImpl> Impl) noexcept
    : Impl(std::move(Impl)) {
  assert(this->Impl && "DeviceContext requires an implementation");
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

std::shared_ptr<const detail::DeviceContextIdentity>
DeviceContext::getIdentity() const noexcept {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getIdentity();
}

bool DeviceContext::ownsDeviceContextIdentity(
    const std::shared_ptr<const detail::DeviceContextIdentity> &Identity)
    const noexcept {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->getIdentity() == Identity;
}

llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
DeviceContext::createHostBufferStorage(size_t SizeInBytes) {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->createHostBufferStorage(SizeInBytes);
}

llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
DeviceContext::enqueueCreateBufferStorage(size_t SizeInBytes) {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->enqueueCreateBufferStorage(SizeInBytes);
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

llvm::Error DeviceContext::synchronize() {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->synchronize();
}

llvm::Expected<bool> DeviceContext::hasPendingWork() const {
  assert(Impl && "cannot use a moved-from DeviceContext");
  return Impl->hasPendingWork();
}
