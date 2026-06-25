//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines typed host and device buffers used by offload operations.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_OFFLOAD_DEVICE_BUFFER_HPP
#define MAGE_OFFLOAD_DEVICE_BUFFER_HPP

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "mage/Offload/DeviceContext.hpp"
#include "mage/Support/TypeTraits.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"

#include <assert.h>
#include <memory>
#include <stddef.h>
#include <utility>

namespace mage {

namespace detail {

class HostBufferStorage {
public:
  virtual ~HostBufferStorage() noexcept;

  HostBufferStorage(const HostBufferStorage &) = delete;
  HostBufferStorage &operator=(const HostBufferStorage &) = delete;
  HostBufferStorage(HostBufferStorage &&) = delete;
  HostBufferStorage &operator=(HostBufferStorage &&) = delete;

  [[nodiscard]] void *data() const noexcept;
  [[nodiscard]] size_t sizeInBytes() const noexcept;

protected:
  HostBufferStorage(void *Data, size_t SizeInBytes) noexcept;

private:
  void *Data;
  size_t SizeInBytes;
};

} // namespace detail

/// Represents a contiguous block of host-resident memory.
template <typename T> class HostBuffer {
  static_assert(is_trivially_copyable_v<T>,
                "HostBuffer elements must be trivially copyable");

public:
  using value_type = T;

  HostBuffer() noexcept = default;
  ~HostBuffer() noexcept = default;

  HostBuffer(const HostBuffer &) = delete;
  HostBuffer &operator=(const HostBuffer &) = delete;
  HostBuffer(HostBuffer &&Other) noexcept
      : Storage(std::move(Other.Storage)),
        ElementCount(std::exchange(Other.ElementCount, 0)) {}
  HostBuffer &operator=(HostBuffer &&Other) noexcept {
    if (this == &Other)
      return *this;

    Storage = std::move(Other.Storage);
    ElementCount = std::exchange(Other.ElementCount, 0);
    return *this;
  }

  [[nodiscard]] T *data() noexcept {
    assert((Storage || ElementCount == 0) &&
           "non-empty HostBuffer requires storage");
    if (!Storage)
      return nullptr;

    return static_cast<T *>(Storage->data());
  }

  [[nodiscard]] const T *data() const noexcept {
    assert((Storage || ElementCount == 0) &&
           "non-empty HostBuffer requires storage");
    if (!Storage)
      return nullptr;

    return static_cast<const T *>(Storage->data());
  }

  [[nodiscard]] size_t size() const noexcept { return ElementCount; }
  [[nodiscard]] bool empty() const noexcept { return ElementCount == 0; }

  operator llvm::ArrayRef<T>() const noexcept {
    return llvm::ArrayRef<T>(data(), size());
  }

  operator llvm::MutableArrayRef<T>() noexcept {
    return llvm::MutableArrayRef<T>(data(), size());
  }

  [[nodiscard]] T &operator[](size_t Index) noexcept {
    assert(Index < size() && "Index must not exceed HostBuffer size");
    return data()[Index];
  }

  [[nodiscard]] const T &operator[](size_t Index) const noexcept {
    assert(Index < size() && "Index must not exceed HostBuffer size");
    return data()[Index];
  }

private:
  friend class DeviceContext;

  HostBuffer(std::shared_ptr<detail::HostBufferStorage> Storage,
             size_t ElementCount) noexcept
      : Storage(std::move(Storage)), ElementCount(ElementCount) {
    assert(this->Storage && "non-empty HostBuffer requires storage");
    assert(this->Storage->sizeInBytes() == (ElementCount * sizeof(T)) &&
           "HostBuffer storage byte size mismatch");
  }

  std::shared_ptr<detail::HostBufferStorage> Storage;
  size_t ElementCount = 0;
};

template <typename T>
llvm::Expected<HostBuffer<T>>
DeviceContext::createHostBuffer(size_t ElementCount) {
  static_assert(is_trivially_copyable_v<T>,
                "HostBuffer elements must be trivially copyable");

  if (ElementCount == 0)
    return HostBuffer<T>();

  if (ElementCount > static_cast<size_t>(-1) / sizeof(T))
    return llvm::createStringError("host buffer allocation size overflows "
                                   "size_t: %zu elements of %zu bytes",
                                   ElementCount, sizeof(T));

  auto StorageOrErr = createHostBufferStorage(ElementCount * sizeof(T));
  if (!StorageOrErr)
    return StorageOrErr.takeError();

  return HostBuffer<T>(std::move(*StorageOrErr), ElementCount);
}

} // namespace mage

#endif // MAGE_OFFLOAD_DEVICE_BUFFER_HPP
