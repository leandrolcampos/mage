//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements non-template storage support for host and device buffers.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/Memory.hpp"

#include <assert.h>
#include <memory>
#include <stddef.h>
#include <utility>

using namespace mage;

detail::HostBufferStorage::~HostBufferStorage() noexcept = default;

DeviceAPI detail::HostBufferStorage::getAPI() const noexcept { return API; }

void *detail::HostBufferStorage::data() noexcept { return Data; }

const void *detail::HostBufferStorage::data() const noexcept { return Data; }

size_t detail::HostBufferStorage::sizeInBytes() const noexcept {
  return SizeInBytes;
}

detail::HostBufferStorage::HostBufferStorage(DeviceAPI API, void *Data,
                                             size_t SizeInBytes) noexcept
    : API(API), Data(Data), SizeInBytes(SizeInBytes) {
  assert((Data || SizeInBytes == 0) && "non-empty storage requires data");
}

detail::DeviceBufferStorage::~DeviceBufferStorage() noexcept = default;

std::shared_ptr<const detail::DeviceIdentity>
detail::DeviceBufferStorage::getDeviceIdentity() const noexcept {
  return OwnerIdentity;
}

void *detail::DeviceBufferStorage::data() noexcept { return Data; }

const void *detail::DeviceBufferStorage::data() const noexcept { return Data; }

size_t detail::DeviceBufferStorage::sizeInBytes() const noexcept {
  return SizeInBytes;
}

detail::DeviceBufferStorage::DeviceBufferStorage(
    std::shared_ptr<const detail::DeviceIdentity> OwnerIdentity, void *Data,
    size_t SizeInBytes) noexcept
    : OwnerIdentity(std::move(OwnerIdentity)), Data(Data),
      SizeInBytes(SizeInBytes) {
  assert(this->OwnerIdentity &&
         "DeviceBufferStorage requires an owner device identity");
  assert((Data || SizeInBytes == 0) && "non-empty storage requires data");
}
