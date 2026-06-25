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

#include "mage/Offload/DeviceBuffer.hpp"

#include <assert.h>
#include <stddef.h>

using namespace mage;

detail::HostBufferStorage::~HostBufferStorage() noexcept = default;

void *detail::HostBufferStorage::data() const noexcept { return Data; }

size_t detail::HostBufferStorage::sizeInBytes() const noexcept {
  return SizeInBytes;
}

detail::HostBufferStorage::HostBufferStorage(void *Data,
                                             size_t SizeInBytes) noexcept
    : Data(Data), SizeInBytes(SizeInBytes) {
  assert((Data || SizeInBytes == 0) && "non-empty storage requires data");
}
