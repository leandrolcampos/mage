//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements non-template support for device modules and functions.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/Module.hpp"

#include "llvm/Support/Error.h"

#include <assert.h>
#include <memory>
#include <utility>

using namespace mage;

detail::DeviceModuleStorage::~DeviceModuleStorage() noexcept = default;

llvm::Expected<int> DeviceModule::getFunctionCount() const {
  if (!Storage)
    return llvm::createStringError(
        "cannot get the function count from a DeviceModule with no loaded "
        "module");

  return Storage->getFunctionCount();
}

llvm::Expected<std::shared_ptr<detail::DeviceFunctionStorage>>
DeviceModule::getFunctionStorage(llvm::StringRef FunctionName) const {
  if (!Storage)
    return llvm::createStringError(
        "cannot get a function from a DeviceModule with no loaded module");

  return Storage->getFunctionStorage(Storage, FunctionName);
}

detail::DeviceFunctionStorage::~DeviceFunctionStorage() noexcept = default;

detail::DeviceFunctionStorage::DeviceFunctionStorage(
    std::shared_ptr<const detail::DeviceModuleStorage> ModuleStorage) noexcept
    : ModuleStorage(std::move(ModuleStorage)) {
  assert(this->ModuleStorage &&
         "DeviceFunctionStorage requires module storage");
}
