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

#ifndef MAGE_OFFLOAD_MEMORY_HPP
#define MAGE_OFFLOAD_MEMORY_HPP

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "mage/Offload/Context.hpp"
#include "mage/Support/TypeTraits.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"

#include <assert.h>
#include <memory>
#include <stddef.h>
#include <utility>

namespace mage {

//===----------------------------------------------------------------------===//
// Host buffers
//===----------------------------------------------------------------------===//

namespace detail {

class HostBufferStorage {
public:
  virtual ~HostBufferStorage() noexcept;

  HostBufferStorage(const HostBufferStorage &) = delete;
  HostBufferStorage &operator=(const HostBufferStorage &) = delete;
  HostBufferStorage(HostBufferStorage &&) = delete;
  HostBufferStorage &operator=(HostBufferStorage &&) = delete;

  [[nodiscard]] void *data() noexcept;
  [[nodiscard]] const void *data() const noexcept;
  [[nodiscard]] size_t sizeInBytes() const noexcept;

protected:
  HostBufferStorage(void *Data, size_t SizeInBytes) noexcept;

private:
  void *Data;
  size_t SizeInBytes;
};

} // namespace detail

/// Represents a typed, contiguous block of host-resident memory.
///
/// Buffers created by DeviceContext::createHostBuffer own host-accessible
/// storage that can be used as the host endpoint of offload transfers.
///
/// A default-constructed, zero-length, or moved-from buffer is empty.
template <typename T> class [[nodiscard]] HostBuffer {
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

  /// Returns the start of the buffer, or nullptr if the buffer is empty.
  [[nodiscard]] T *data() noexcept {
    assert((Storage || ElementCount == 0) &&
           "non-empty HostBuffer requires storage");
    if (!Storage)
      return nullptr;

    return static_cast<T *>(Storage->data());
  }

  /// Returns the start of the buffer, or nullptr if the buffer is empty.
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

  /// Returns the element at \p Index.
  ///
  /// \p Index must be less than size().
  [[nodiscard]] T &operator[](size_t Index) noexcept {
    assert(Index < size() && "Index must not exceed HostBuffer size");
    return data()[Index];
  }

  /// Returns the element at \p Index.
  ///
  /// \p Index must be less than size().
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

//===----------------------------------------------------------------------===//
// Device buffers
//===----------------------------------------------------------------------===//

namespace detail {

class DeviceBufferStorage {
public:
  virtual ~DeviceBufferStorage() noexcept;

  DeviceBufferStorage(const DeviceBufferStorage &) = delete;
  DeviceBufferStorage &operator=(const DeviceBufferStorage &) = delete;
  DeviceBufferStorage(DeviceBufferStorage &&) = delete;
  DeviceBufferStorage &operator=(DeviceBufferStorage &&) = delete;

  [[nodiscard]] std::shared_ptr<const DeviceIdentity>
  getDeviceIdentity() const noexcept;
  [[nodiscard]] void *data() noexcept;
  [[nodiscard]] const void *data() const noexcept;
  [[nodiscard]] size_t sizeInBytes() const noexcept;

protected:
  DeviceBufferStorage(std::shared_ptr<const DeviceIdentity> OwnerIdentity,
                      void *Data, size_t SizeInBytes) noexcept;

private:
  std::shared_ptr<const DeviceIdentity> OwnerIdentity;
  void *Data;
  size_t SizeInBytes;
};

} // namespace detail

/// Represents a typed, contiguous block of device-resident global memory.
///
/// Buffers created by DeviceContext::createBuffer own device storage and remain
/// bound to the device associated with the creating context.
///
/// A default-constructed, zero-length, or moved-from buffer is empty.
template <typename T> class [[nodiscard]] DeviceBuffer {
  static_assert(is_trivially_copyable_v<T>,
                "DeviceBuffer elements must be trivially copyable");

public:
  using value_type = T;

  DeviceBuffer() noexcept = default;
  ~DeviceBuffer() noexcept = default;

  DeviceBuffer(const DeviceBuffer &) = delete;
  DeviceBuffer &operator=(const DeviceBuffer &) = delete;

  DeviceBuffer(DeviceBuffer &&Other) noexcept
      : Storage(std::move(Other.Storage)),
        ElementCount(std::exchange(Other.ElementCount, 0)) {}

  DeviceBuffer &operator=(DeviceBuffer &&Other) noexcept {
    if (this == &Other)
      return *this;

    Storage = std::move(Other.Storage);
    ElementCount = std::exchange(Other.ElementCount, 0);
    return *this;
  }

  [[nodiscard]] T *data() noexcept {
    assert((Storage || ElementCount == 0) &&
           "non-empty DeviceBuffer requires storage");
    if (!Storage)
      return nullptr;

    return static_cast<T *>(Storage->data());
  }

  [[nodiscard]] const T *data() const noexcept {
    assert((Storage || ElementCount == 0) &&
           "non-empty DeviceBuffer requires storage");
    if (!Storage)
      return nullptr;

    return static_cast<const T *>(Storage->data());
  }

  [[nodiscard]] size_t size() const noexcept { return ElementCount; }
  [[nodiscard]] bool empty() const noexcept { return ElementCount == 0; }

private:
  friend class DeviceContext;

  DeviceBuffer(std::shared_ptr<detail::DeviceBufferStorage> Storage,
               size_t ElementCount) noexcept
      : Storage(std::move(Storage)), ElementCount(ElementCount) {
    assert(this->Storage && "non-empty DeviceBuffer requires storage");
    assert(this->Storage->sizeInBytes() == (ElementCount * sizeof(T)) &&
           "DeviceBuffer storage byte size mismatch");
  }

  std::shared_ptr<detail::DeviceBufferStorage> Storage;
  size_t ElementCount = 0;
};

template <typename T>
llvm::Expected<DeviceBuffer<T>>
DeviceContext::createBuffer(size_t ElementCount) {
  static_assert(is_trivially_copyable_v<T>,
                "DeviceBuffer elements must be trivially copyable");

  if (ElementCount == 0)
    return DeviceBuffer<T>();

  if (ElementCount > static_cast<size_t>(-1) / sizeof(T))
    return llvm::createStringError("device buffer allocation size overflows "
                                   "size_t: %zu elements of %zu bytes",
                                   ElementCount, sizeof(T));

  auto StorageOrErr = createBufferStorage(ElementCount * sizeof(T));
  if (!StorageOrErr)
    return StorageOrErr.takeError();

  return DeviceBuffer<T>(std::move(*StorageOrErr), ElementCount);
}

//===----------------------------------------------------------------------===//
// Buffer copy operations
//===----------------------------------------------------------------------===//

template <typename T>
llvm::Error DeviceContext::enqueueCopy(DeviceBuffer<T> &Dst,
                                       const HostBuffer<T> &Src) {
  static_assert(is_trivially_copyable_v<T>,
                "buffer elements must be trivially copyable");

  if (Dst.empty())
    return llvm::Error::success();

  if (Src.size() < Dst.size())
    return llvm::createStringError(
        "host-to-device copy requires the source buffer to contain at least "
        "%zu elements; source contains %zu",
        Dst.size(), Src.size());

  assert(Dst.Storage && "non-empty DeviceBuffer requires storage");
  assert(Src.Storage && "non-empty HostBuffer requires storage");

  if (!ownsDeviceIdentity(Dst.Storage->getDeviceIdentity()))
    return llvm::createStringError(
        "host-to-device copy requires the destination device buffer to have "
        "been created on this DeviceContext's device");

  return enqueueCopyToDeviceStorage(Dst.Storage, Src.Storage,
                                    Dst.size() * sizeof(T));
}

template <typename T>
llvm::Error DeviceContext::enqueueCopy(HostBuffer<T> &Dst,
                                       const DeviceBuffer<T> &Src) {
  static_assert(is_trivially_copyable_v<T>,
                "buffer elements must be trivially copyable");

  if (Dst.empty())
    return llvm::Error::success();

  if (Src.size() < Dst.size())
    return llvm::createStringError(
        "device-to-host copy requires the source buffer to contain at least "
        "%zu elements; source contains %zu",
        Dst.size(), Src.size());

  assert(Dst.Storage && "non-empty HostBuffer requires storage");
  assert(Src.Storage && "non-empty DeviceBuffer requires storage");

  if (!ownsDeviceIdentity(Src.Storage->getDeviceIdentity()))
    return llvm::createStringError(
        "device-to-host copy requires the source device buffer to have been "
        "created on this DeviceContext's device");

  return enqueueCopyToHostStorage(Dst.Storage, Src.Storage,
                                  Dst.size() * sizeof(T));
}

} // namespace mage

#endif // MAGE_OFFLOAD_MEMORY_HPP
