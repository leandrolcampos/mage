//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines device modules and typed device functions used by offload
/// operations.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_OFFLOAD_MODULE_HPP
#define MAGE_OFFLOAD_MODULE_HPP

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <assert.h>
#include <memory>
#include <utility>

namespace mage {

class DeviceContext;
template <typename FuncType> class [[nodiscard]] DeviceFunction;

//===----------------------------------------------------------------------===//
// Device modules
//===----------------------------------------------------------------------===//

namespace detail {

class DeviceIdentity;
class DeviceFunctionStorage;

class DeviceModuleStorage {
public:
  virtual ~DeviceModuleStorage() noexcept;

  DeviceModuleStorage(const DeviceModuleStorage &) = delete;
  DeviceModuleStorage &operator=(const DeviceModuleStorage &) = delete;
  DeviceModuleStorage(DeviceModuleStorage &&) = delete;
  DeviceModuleStorage &operator=(DeviceModuleStorage &&) = delete;

  virtual llvm::Expected<int> getFunctionCount() const = 0;
  virtual llvm::Expected<std::shared_ptr<DeviceFunctionStorage>>
  getFunctionStorage(std::shared_ptr<const DeviceModuleStorage> ModuleStorage,
                     llvm::StringRef FunctionName) const = 0;

protected:
  DeviceModuleStorage() noexcept = default;
};

} // namespace detail

/// Represents a module loaded onto a device.
///
/// Modules are loaded from a device image by DeviceContext::loadModule and
/// remain bound to the device that loaded them.
///
/// A default-constructed or moved-from module contains no loaded module.
class [[nodiscard]] DeviceModule {
public:
  DeviceModule() noexcept = default;
  ~DeviceModule() noexcept = default;

  DeviceModule(const DeviceModule &) = delete;
  DeviceModule &operator=(const DeviceModule &) = delete;

  DeviceModule(DeviceModule &&Other) noexcept
      : Storage(std::move(Other.Storage)),
        OwnerIdentity(std::move(Other.OwnerIdentity)) {}

  DeviceModule &operator=(DeviceModule &&Other) noexcept {
    if (this == &Other)
      return *this;

    Storage = std::move(Other.Storage);
    OwnerIdentity = std::move(Other.OwnerIdentity);
    return *this;
  }

  /// Returns the number of launchable functions exported by this module.
  llvm::Expected<int> getFunctionCount() const;

  /// Returns the launchable function named \p FunctionName from this module.
  ///
  /// The returned function keeps the loaded module resource and the resolved
  /// function handle alive, and is bound to the same device as this module.
  template <typename FuncType>
  llvm::Expected<DeviceFunction<FuncType>>
  getFunction(llvm::StringRef FunctionName) const;

private:
  friend class DeviceContext;

  DeviceModule(
      std::shared_ptr<detail::DeviceModuleStorage> Storage,
      std::shared_ptr<const detail::DeviceIdentity> OwnerIdentity) noexcept
      : Storage(std::move(Storage)), OwnerIdentity(std::move(OwnerIdentity)) {
    assert(this->Storage && "DeviceModule requires storage");
    assert(this->OwnerIdentity &&
           "DeviceModule requires an owner device identity");
  }

  llvm::Expected<std::shared_ptr<detail::DeviceFunctionStorage>>
  getFunctionStorage(llvm::StringRef FunctionName) const;

  std::shared_ptr<detail::DeviceModuleStorage> Storage;
  std::shared_ptr<const detail::DeviceIdentity> OwnerIdentity;
};

//===----------------------------------------------------------------------===//
// Device functions
//===----------------------------------------------------------------------===//

namespace detail {

class DeviceFunctionStorage {
public:
  virtual ~DeviceFunctionStorage() noexcept;

  DeviceFunctionStorage(const DeviceFunctionStorage &) = delete;
  DeviceFunctionStorage &operator=(const DeviceFunctionStorage &) = delete;
  DeviceFunctionStorage(DeviceFunctionStorage &&) = delete;
  DeviceFunctionStorage &operator=(DeviceFunctionStorage &&) = delete;

protected:
  explicit DeviceFunctionStorage(
      std::shared_ptr<const detail::DeviceModuleStorage>
          ModuleStorage) noexcept;

private:
  std::shared_ptr<const detail::DeviceModuleStorage> ModuleStorage;
};

} // namespace detail

/// Represents a typed function resolved from a device module.
///
/// A resolved function keeps the loaded module resource and backend function
/// handle alive, and remains bound to the same device as its parent module.
///
/// A default-constructed or moved-from function has no resolved backend
/// function handle.
template <typename FuncType> class [[nodiscard]] DeviceFunction {
public:
  using function_type = FuncType;

  DeviceFunction() noexcept = default;
  ~DeviceFunction() noexcept = default;

  DeviceFunction(const DeviceFunction &) = delete;
  DeviceFunction &operator=(const DeviceFunction &) = delete;

  DeviceFunction(DeviceFunction &&Other) noexcept
      : Storage(std::move(Other.Storage)),
        OwnerIdentity(std::move(Other.OwnerIdentity)) {}

  DeviceFunction &operator=(DeviceFunction &&Other) noexcept {
    if (this == &Other)
      return *this;

    Storage = std::move(Other.Storage);
    OwnerIdentity = std::move(Other.OwnerIdentity);
    return *this;
  }

private:
  friend class DeviceModule;

  DeviceFunction(
      std::shared_ptr<detail::DeviceFunctionStorage> Storage,
      std::shared_ptr<const detail::DeviceIdentity> OwnerIdentity) noexcept
      : Storage(std::move(Storage)), OwnerIdentity(std::move(OwnerIdentity)) {
    assert(this->Storage && "DeviceFunction requires function storage");
    assert(this->OwnerIdentity &&
           "DeviceFunction requires an owner device identity");
  }

  std::shared_ptr<detail::DeviceFunctionStorage> Storage;
  std::shared_ptr<const detail::DeviceIdentity> OwnerIdentity;
};

template <typename FuncType>
llvm::Expected<DeviceFunction<FuncType>>
DeviceModule::getFunction(llvm::StringRef FunctionName) const {
  auto StorageOrErr = getFunctionStorage(FunctionName);
  if (!StorageOrErr)
    return StorageOrErr.takeError();

  assert(OwnerIdentity && "DeviceModule requires an owner device identity");
  return DeviceFunction<FuncType>(std::move(*StorageOrErr), OwnerIdentity);
}

} // namespace mage

#endif // MAGE_OFFLOAD_MODULE_HPP
