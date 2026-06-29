//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements backend selection and shared backend utilities.
///
//===----------------------------------------------------------------------===//

#include "Backend.hpp"

#if MAGE_CUDA_BACKEND_ENABLED
#include "CUDABackend.hpp"
#endif

#if MAGE_HIP_BACKEND_ENABLED
#include "HIPBackend.hpp"
#endif

#include "mage/Offload/Context.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"

#include <assert.h>
#include <memory>
#include <mutex>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

using namespace mage;

detail::DeviceState::~DeviceState() noexcept = default;

DeviceAPI detail::DeviceState::getAPI() const noexcept { return API; }

int detail::DeviceState::getID() const noexcept { return DeviceID; }

llvm::StringRef detail::DeviceState::getName() const noexcept { return Name; }

llvm::StringRef detail::DeviceState::getArchitecture() const noexcept {
  return Architecture;
}

detail::DeviceState::DeviceState(DeviceAPI API, int DeviceID, std::string Name,
                                 std::string Architecture)
    : API(API), DeviceID(DeviceID), Name(std::move(Name)),
      Architecture(std::move(Architecture)) {}

detail::StreamState::~StreamState() noexcept = default;

detail::DeviceContextImpl::~DeviceContextImpl() noexcept = default;

std::shared_ptr<const detail::DeviceContextIdentity>
detail::DeviceContextImpl::getIdentity() const noexcept {
  return Identity;
}

void detail::DeviceContextImpl::retainPendingResource(
    std::shared_ptr<const void> Resource) {
  assert(Resource && "cannot retain a null pending resource");

  std::lock_guard<std::mutex> Lock(PendingResourcesMutex);
  PendingResources.push_back(std::move(Resource));
}

size_t detail::DeviceContextImpl::releasePendingResources() noexcept {
  std::vector<std::shared_ptr<const void>> Resources;

  {
    std::lock_guard<std::mutex> Lock(PendingResourcesMutex);
    Resources.swap(PendingResources);
  }

  size_t Count = Resources.size();
  Resources.clear();
  return Count;
}

detail::DeviceContextImpl::DeviceContextImpl()
    : Identity(std::make_shared<detail::DeviceContextIdentity>()) {}

int detail::Backend::getAPIVersion() const noexcept { return APIVersion; }

int detail::Backend::getDeviceCount() const noexcept { return DeviceCount; }

bool detail::isBackendEnabled(DeviceAPI API) noexcept {
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
