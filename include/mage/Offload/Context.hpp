//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares DeviceContext and related offload APIs.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_OFFLOAD_CONTEXT_HPP
#define MAGE_OFFLOAD_CONTEXT_HPP

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <memory>
#include <stddef.h>
#include <string>
#include <utility>

namespace mage {

template <typename T> class [[nodiscard]] HostBuffer;
template <typename T> class [[nodiscard]] DeviceBuffer;
class [[nodiscard]] DeviceModule;

enum class DeviceAPI {
  CUDA,
  HIP,
};

[[nodiscard]] const char *toString(DeviceAPI API) noexcept;

/// Returns the number of devices available through \p API.
///
/// Returns 0 if Mage was built without runtime support for \p API.
llvm::Expected<int> getDeviceCount(DeviceAPI API);

namespace detail {
class DeviceIdentity;
class DeviceContextImpl;
class HostBufferStorage;
class DeviceBufferStorage;
class DeviceModuleStorage;
} // namespace detail

/// Represents a single stream of execution on a particular GPU.
class [[nodiscard]] DeviceContext {
public:
  ~DeviceContext() noexcept;

  DeviceContext(const DeviceContext &) = delete;
  DeviceContext &operator=(const DeviceContext &) = delete;
  DeviceContext(DeviceContext &&) noexcept;
  DeviceContext &operator=(DeviceContext &&) noexcept;

  static llvm::Expected<DeviceContext> create(DeviceAPI API, int DeviceID = 0);

  /// Returns the API used to handle the device associated with this context.
  [[nodiscard]] DeviceAPI getAPI() const noexcept;

  /// Returns the ID of the device associated with this context.
  [[nodiscard]] int getID() const noexcept;

  /// Returns the name of the device associated with this context.
  [[nodiscard]] std::string getName() const;

  /// Returns the architecture of the device associated with this context.
  [[nodiscard]] std::string getArchitecture() const;

  /// Returns the free and total memory size of the device associated with this
  /// context.
  llvm::Expected<std::pair<size_t, size_t>> getMemoryInfo() const;

  /// Creates a host buffer containing \p ElementCount values.
  ///
  /// Non-empty host buffers are bound to the API used to handle the device
  /// associated with this context.
  ///
  /// Non-empty host buffers own page-locked (pinned) host memory that can be
  /// used efficiently as the host endpoint of transfers using that API.
  template <typename T>
  llvm::Expected<HostBuffer<T>> createHostBuffer(size_t ElementCount);

  /// Creates a device buffer synchronously containing \p ElementCount values.
  ///
  /// Non-empty buffers are bound to the device associated with this context.
  template <typename T>
  llvm::Expected<DeviceBuffer<T>> createBuffer(size_t ElementCount);

  /// Enqueues a copy from \p Src to \p Dst.
  ///
  /// The number of elements copied is determined by the size of \p Dst;
  /// \p Src must contain at least as many elements.
  ///
  /// Non-empty \p Dst must be bound to the device associated with this context.
  ///
  /// Non-empty \p Src must be bound to the API used to handle the device
  /// associated with this context.
  ///
  /// The underlying storage for both buffers is retained by the context
  /// and released during synchronization after the copy completes.
  template <typename T>
  llvm::Error enqueueCopy(DeviceBuffer<T> &Dst, const HostBuffer<T> &Src);

  /// Enqueues a copy from \p Src to \p Dst.
  ///
  /// The number of elements copied is determined by the size of \p Dst;
  /// \p Src must contain at least as many elements.
  ///
  /// Non-empty \p Dst must be bound to the API used to handle the device
  /// associated with this context.
  ///
  /// Non-empty \p Src must be bound to the device associated with this context.
  ///
  /// The underlying storage for both buffers is retained by the context
  /// and released during synchronization after the copy completes.
  template <typename T>
  llvm::Error enqueueCopy(HostBuffer<T> &Dst, const DeviceBuffer<T> &Src);

  /// Loads a device image onto the device associated with this context.
  ///
  /// The returned module remains bound to that device.
  llvm::Expected<DeviceModule> loadModule(llvm::StringRef ImagePath);

  /// Blocks until all asynchronous calls on the underlying stream have
  /// completed.
  llvm::Error synchronize();

  /// Returns true if this stream has previously enqueued work that has not
  /// completed. Does not block.
  llvm::Expected<bool> hasPendingWork() const;

private:
  explicit DeviceContext(
      std::unique_ptr<detail::DeviceContextImpl> Impl) noexcept;

  [[nodiscard]] bool ownsDeviceIdentity(
      const std::shared_ptr<const detail::DeviceIdentity> &Identity)
      const noexcept;

  llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
  createHostBufferStorage(size_t SizeInBytes);

  llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
  createBufferStorage(size_t SizeInBytes);

  llvm::Error enqueueCopyToDeviceStorage(
      std::shared_ptr<detail::DeviceBufferStorage> Dst,
      std::shared_ptr<const detail::HostBufferStorage> Src, size_t SizeInBytes);

  llvm::Error enqueueCopyToHostStorage(
      std::shared_ptr<detail::HostBufferStorage> Dst,
      std::shared_ptr<const detail::DeviceBufferStorage> Src,
      size_t SizeInBytes);

  std::unique_ptr<detail::DeviceContextImpl> Impl;
};

} // namespace mage

#endif // MAGE_OFFLOAD_CONTEXT_HPP
