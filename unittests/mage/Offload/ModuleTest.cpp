//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests device modules and typed device functions used by offload operations.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/Module.hpp"
#include "UnitTest/Test.hpp"

#include "mage/Offload/Context.hpp"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include <string>
#include <type_traits>

using namespace mage;

namespace {
constexpr DeviceAPI DeviceAPIs[] = {DeviceAPI::CUDA, DeviceAPI::HIP};
} // namespace

template <typename Func> static void forEachDeviceAPI(Func &&Fn) {
  for (DeviceAPI API : DeviceAPIs)
    Fn(API);
}

static const char *getDeviceImageTargetTriple(DeviceAPI API) noexcept {
  switch (API) {
  case DeviceAPI::CUDA:
    return "nvptx64-nvidia-cuda";
  case DeviceAPI::HIP:
    return "amdgcn-amd-amdhsa";
  }

  return "";
}

static llvm::SmallString<256> getDeviceImagePath(DeviceAPI API) {
  llvm::SmallString<256> Path(
      MAGE_DEVICE_IMAGE_OFFLOADMODULETESTDEVICEIMAGE_DIR);

  std::string FileName =
      std::string(MAGE_DEVICE_IMAGE_OFFLOADMODULETESTDEVICEIMAGE_FILE_STEM) +
      "." + getDeviceImageTargetTriple(API) + ".bin";
  llvm::sys::path::append(Path, FileName);
  return Path;
}

static_assert(!std::is_copy_constructible<DeviceModule>::value,
              "DeviceModule must not be copy constructible");
static_assert(!std::is_copy_assignable<DeviceModule>::value,
              "DeviceModule must not be copy assignable");
static_assert(std::is_move_constructible<DeviceModule>::value,
              "DeviceModule must be move constructible");
static_assert(std::is_move_assignable<DeviceModule>::value,
              "DeviceModule must be move assignable");

using DoNothingFunction = void();
using UseRegistersFunction = void(int *);

static_assert(
    !std::is_copy_constructible<DeviceFunction<DoNothingFunction>>::value,
    "DeviceFunction must not be copy constructible");
static_assert(
    !std::is_copy_assignable<DeviceFunction<DoNothingFunction>>::value,
    "DeviceFunction must not be copy assignable");
static_assert(
    std::is_move_constructible<DeviceFunction<DoNothingFunction>>::value,
    "DeviceFunction must be move constructible");
static_assert(std::is_move_assignable<DeviceFunction<DoNothingFunction>>::value,
              "DeviceFunction must be move assignable");

MAGE_TEST(ModuleTest, LoadsModulesForAvailableDeviceImages) {
  forEachDeviceAPI([&](DeviceAPI API) {
    llvm::SmallString<256> ImagePath = getDeviceImagePath(API);
    if (!llvm::sys::fs::exists(ImagePath))
      return;

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    for (int DeviceID = 0; DeviceID < *CountOrErr; ++DeviceID) {
      auto ContextOrErr = DeviceContext::create(API, DeviceID);
      if (!ContextOrErr) {
        llvm::consumeError(ContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto ModuleOrErr = ContextOrErr->loadModule(ImagePath);
      if (!ModuleOrErr) {
        llvm::consumeError(ModuleOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto FunctionCountOrErr = ModuleOrErr->getFunctionCount();
      if (!FunctionCountOrErr) {
        llvm::consumeError(FunctionCountOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      MAGE_EXPECT_EQ(*FunctionCountOrErr, 2);
    }
  });
}

MAGE_TEST(ModuleTest, GetsLaunchableFunctionsForAvailableDeviceImages) {
  forEachDeviceAPI([&](DeviceAPI API) {
    llvm::SmallString<256> ImagePath = getDeviceImagePath(API);
    if (!llvm::sys::fs::exists(ImagePath))
      return;

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    for (int DeviceID = 0; DeviceID < *CountOrErr; ++DeviceID) {
      auto ContextOrErr = DeviceContext::create(API, DeviceID);
      if (!ContextOrErr) {
        llvm::consumeError(ContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto ModuleOrErr = ContextOrErr->loadModule(ImagePath);
      if (!ModuleOrErr) {
        llvm::consumeError(ModuleOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto DoNothingOrErr =
          ModuleOrErr->getFunction<DoNothingFunction>("moduleTestDoNothing");
      if (!DoNothingOrErr) {
        llvm::consumeError(DoNothingOrErr.takeError());
        MAGE_EXPECT_TRUE(false);
      }

      auto UseRegistersOrErr = ModuleOrErr->getFunction<UseRegistersFunction>(
          "moduleTestUseRegisters");
      if (!UseRegistersOrErr) {
        llvm::consumeError(UseRegistersOrErr.takeError());
        MAGE_EXPECT_TRUE(false);
      }
    }
  });
}

MAGE_TEST(ModuleTest, RejectsNonLaunchableFunctions) {
  forEachDeviceAPI([&](DeviceAPI API) {
    llvm::SmallString<256> ImagePath = getDeviceImagePath(API);
    if (!llvm::sys::fs::exists(ImagePath))
      return;

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    for (int DeviceID = 0; DeviceID < *CountOrErr; ++DeviceID) {
      auto ContextOrErr = DeviceContext::create(API, DeviceID);
      if (!ContextOrErr) {
        llvm::consumeError(ContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto ModuleOrErr = ContextOrErr->loadModule(ImagePath);
      if (!ModuleOrErr) {
        llvm::consumeError(ModuleOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto FunctionOrErr =
          ModuleOrErr->getFunction<int(int)>("moduleTestNotLaunchable");
      if (!FunctionOrErr)
        llvm::consumeError(FunctionOrErr.takeError());
      else
        MAGE_EXPECT_TRUE(false);
    }
  });
}

MAGE_TEST(ModuleTest, RejectsMissingDeviceImages) {
  forEachDeviceAPI([&](DeviceAPI API) {
    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    if (*CountOrErr == 0)
      return;

    auto ContextOrErr = DeviceContext::create(API);
    if (!ContextOrErr) {
      llvm::consumeError(ContextOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto ModuleOrErr = ContextOrErr->loadModule("missing-device-image.bin");
    if (!ModuleOrErr)
      llvm::consumeError(ModuleOrErr.takeError());
    else
      MAGE_EXPECT_TRUE(false);
  });
}
