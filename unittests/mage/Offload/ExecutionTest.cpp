//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests device function launch APIs.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/Execution.hpp"
#include "UnitTest/Test.hpp"

#include "mage/Offload/Context.hpp"
#include "mage/Offload/Memory.hpp"
#include "mage/Offload/Module.hpp"
#include "mage/Support/TypeTraits.hpp"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>

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
      MAGE_DEVICE_IMAGE_OFFLOADEXECUTIONTESTDEVICEIMAGE_DIR);

  std::string FileName =
      std::string(MAGE_DEVICE_IMAGE_OFFLOADEXECUTIONTESTDEVICEIMAGE_FILE_STEM) +
      "." + getDeviceImageTargetTriple(API) + ".bin";
  llvm::sys::path::append(Path, FileName);
  return Path;
}

using DoNothingFunction = void();
using ScaleFunction = void(int *, unsigned int, int);
using AddFunction = void(const int *, int *, unsigned int, int);

static_assert(detail::is_launch_arg_compatible<int, int &&>::value,
              "exact scalar launch argument types are compatible");
static_assert(detail::is_launch_arg_compatible<int, const int &>::value,
              "const scalar lvalue arguments are copied by value");
static_assert(!detail::is_launch_arg_compatible<int, long &&>::value,
              "scalar launch argument types must match exactly");
static_assert(!detail::is_launch_arg_compatible<int *, int *&&>::value,
              "raw pointer launch arguments are rejected");
static_assert(!detail::is_launch_arg_compatible<const int *, int *&&>::value,
              "raw pointer launch arguments are rejected even when pointee "
              "const qualification could be added");
static_assert(!detail::is_launch_arg_compatible<int *, const int *&&>::value,
              "launch arguments must not remove pointee const qualification");
static_assert(
    detail::is_launch_arg_compatible<int *, DeviceBuffer<int> &>::value,
    "DeviceBuffer<T> lvalues can satisfy T * launch arguments");
static_assert(
    detail::is_launch_arg_compatible<const int *, DeviceBuffer<int> &>::value,
    "DeviceBuffer<T> lvalues can satisfy const T * launch arguments");
static_assert(
    detail::is_launch_arg_compatible<const int *,
                                     const DeviceBuffer<int> &>::value,
    "const DeviceBuffer<T> lvalues can satisfy const T * launch arguments");
static_assert(
    !detail::is_launch_arg_compatible<int *, const DeviceBuffer<int> &>::value,
    "const DeviceBuffer<T> lvalues cannot satisfy mutable T * launch "
    "arguments");
static_assert(
    !detail::is_launch_arg_compatible<float *, DeviceBuffer<int> &>::value,
    "DeviceBuffer<T> launch arguments preserve the element type");
static_assert(detail::are_launch_args_compatible<
                  function_parameter_types_t<ScaleFunction>,
                  DeviceBuffer<int> &, unsigned int &&, int &&>::value,
              "launch arguments are checked pairwise against function "
              "argument types");
static_assert(
    !detail::are_launch_args_compatible<
        function_parameter_types_t<ScaleFunction>, DeviceBuffer<int> &>::value,
    "launch argument count must match the function signature");
static_assert(detail::are_launch_args_compatible<
                  function_parameter_types_t<DoNothingFunction>>::value,
              "empty launch argument lists are compatible with no-argument "
              "functions");

MAGE_TEST(ExecutionTest, LaunchesFunctionWithoutArguments) {
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

      DeviceContext &Context = *ContextOrErr;

      auto ModuleOrErr = Context.loadModule(ImagePath);
      if (!ModuleOrErr) {
        llvm::consumeError(ModuleOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceModule &Module = *ModuleOrErr;

      auto DoNothingOrErr =
          Module.getFunction<DoNothingFunction>("executionTestDoNothing");
      if (!DoNothingOrErr) {
        llvm::consumeError(DoNothingOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = Context.enqueueLaunch(*DoNothingOrErr, LaunchConfig{})) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = Context.synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }
    }
  });
}

MAGE_TEST(ExecutionTest, LaunchesFunctionWithMutableDeviceBufferArgument) {
  forEachDeviceAPI([&](DeviceAPI API) {
    llvm::SmallString<256> ImagePath = getDeviceImagePath(API);
    if (!llvm::sys::fs::exists(ImagePath))
      return;

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    constexpr size_t ElementCount = 4;

    for (int DeviceID = 0; DeviceID < *CountOrErr; ++DeviceID) {
      auto ContextOrErr = DeviceContext::create(API, DeviceID);
      if (!ContextOrErr) {
        llvm::consumeError(ContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceContext &Context = *ContextOrErr;

      auto ModuleOrErr = Context.loadModule(ImagePath);
      if (!ModuleOrErr) {
        llvm::consumeError(ModuleOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceModule &Module = *ModuleOrErr;

      auto ScaleOrErr = Module.getFunction<ScaleFunction>("executionTestScale");
      if (!ScaleOrErr) {
        llvm::consumeError(ScaleOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto DeviceOutputOrErr = Context.createBuffer<int>(ElementCount);
      if (!DeviceOutputOrErr) {
        llvm::consumeError(DeviceOutputOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto HostOutputOrErr = Context.createHostBuffer<int>(ElementCount);
      if (!HostOutputOrErr) {
        llvm::consumeError(HostOutputOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceBuffer<int> &DeviceOutput = *DeviceOutputOrErr;
      HostBuffer<int> &HostOutput = *HostOutputOrErr;

      LaunchConfig Config;
      Config.BlockDim.X = static_cast<uint32_t>(ElementCount);

      if (auto Err = Context.enqueueLaunch(
              *ScaleOrErr, Config, DeviceOutput,
              static_cast<unsigned int>(ElementCount), 7)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = Context.enqueueCopy(HostOutput, DeviceOutput)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = Context.synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      MAGE_EXPECT_EQ(HostOutput[0], 0);
      MAGE_EXPECT_EQ(HostOutput[1], 7);
      MAGE_EXPECT_EQ(HostOutput[2], 14);
      MAGE_EXPECT_EQ(HostOutput[3], 21);
    }
  });
}

MAGE_TEST(ExecutionTest, LaunchesFunctionWithConstDeviceBufferArgument) {
  forEachDeviceAPI([&](DeviceAPI API) {
    llvm::SmallString<256> ImagePath = getDeviceImagePath(API);
    if (!llvm::sys::fs::exists(ImagePath))
      return;

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    constexpr size_t ElementCount = 4;

    for (int DeviceID = 0; DeviceID < *CountOrErr; ++DeviceID) {
      auto ContextOrErr = DeviceContext::create(API, DeviceID);
      if (!ContextOrErr) {
        llvm::consumeError(ContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceContext &Context = *ContextOrErr;

      auto ModuleOrErr = Context.loadModule(ImagePath);
      if (!ModuleOrErr) {
        llvm::consumeError(ModuleOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceModule &Module = *ModuleOrErr;

      auto AddOrErr = Module.getFunction<AddFunction>("executionTestAdd");
      if (!AddOrErr) {
        llvm::consumeError(AddOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto DeviceInputOrErr = Context.createBuffer<int>(ElementCount);
      if (!DeviceInputOrErr) {
        llvm::consumeError(DeviceInputOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto DeviceOutputOrErr = Context.createBuffer<int>(ElementCount);
      if (!DeviceOutputOrErr) {
        llvm::consumeError(DeviceOutputOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto HostInputOrErr = Context.createHostBuffer<int>(ElementCount);
      if (!HostInputOrErr) {
        llvm::consumeError(HostInputOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto HostOutputOrErr = Context.createHostBuffer<int>(ElementCount);
      if (!HostOutputOrErr) {
        llvm::consumeError(HostOutputOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceBuffer<int> &DeviceInput = *DeviceInputOrErr;
      DeviceBuffer<int> &DeviceOutput = *DeviceOutputOrErr;
      HostBuffer<int> &HostInput = *HostInputOrErr;
      HostBuffer<int> &HostOutput = *HostOutputOrErr;

      HostInput[0] = 3;
      HostInput[1] = 1;
      HostInput[2] = 4;
      HostInput[3] = 2;

      if (auto Err = Context.enqueueCopy(DeviceInput, HostInput)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      LaunchConfig Config;
      Config.BlockDim.X = static_cast<uint32_t>(ElementCount);

      const DeviceBuffer<int> &ConstDeviceInput = DeviceInput;
      if (auto Err = Context.enqueueLaunch(
              *AddOrErr, Config, ConstDeviceInput, DeviceOutput,
              static_cast<unsigned int>(ElementCount), 5)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = Context.enqueueCopy(HostOutput, DeviceOutput)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = Context.synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      MAGE_EXPECT_EQ(HostOutput[0], 8);
      MAGE_EXPECT_EQ(HostOutput[1], 6);
      MAGE_EXPECT_EQ(HostOutput[2], 9);
      MAGE_EXPECT_EQ(HostOutput[3], 7);
    }
  });
}

MAGE_TEST(ExecutionTest, RejectsForeignDeviceBufferLaunchArguments) {
  forEachDeviceAPI([&](DeviceAPI API) {
    llvm::SmallString<256> ImagePath = getDeviceImagePath(API);
    if (!llvm::sys::fs::exists(ImagePath))
      return;

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    if (*CountOrErr < 2)
      return;

    auto ContextOrErr = DeviceContext::create(API, 0);
    if (!ContextOrErr) {
      llvm::consumeError(ContextOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto OtherContextOrErr = DeviceContext::create(API, 1);
    if (!OtherContextOrErr) {
      llvm::consumeError(OtherContextOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    DeviceContext &Context = *ContextOrErr;
    DeviceContext &OtherContext = *OtherContextOrErr;

    auto ModuleOrErr = Context.loadModule(ImagePath);
    if (!ModuleOrErr) {
      llvm::consumeError(ModuleOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto FunctionOrErr =
        ModuleOrErr->getFunction<ScaleFunction>("executionTestScale");
    if (!FunctionOrErr) {
      llvm::consumeError(FunctionOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    constexpr size_t ElementCount = 4;
    auto ForeignBufferOrErr = OtherContext.createBuffer<int>(ElementCount);
    if (!ForeignBufferOrErr) {
      llvm::consumeError(ForeignBufferOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    LaunchConfig Config;
    Config.BlockDim.X = static_cast<uint32_t>(ElementCount);

    if (auto Err =
            Context.enqueueLaunch(*FunctionOrErr, Config, *ForeignBufferOrErr,
                                  static_cast<unsigned int>(ElementCount), 7)) {
      llvm::consumeError(std::move(Err));
    } else
      MAGE_EXPECT_TRUE(false);
  });
}

MAGE_TEST(ExecutionTest, RejectsForeignDeviceFunctions) {
  forEachDeviceAPI([&](DeviceAPI API) {
    llvm::SmallString<256> ImagePath = getDeviceImagePath(API);
    if (!llvm::sys::fs::exists(ImagePath))
      return;

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    if (*CountOrErr < 2)
      return;

    auto ContextOrErr = DeviceContext::create(API, 0);
    if (!ContextOrErr) {
      llvm::consumeError(ContextOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto OtherContextOrErr = DeviceContext::create(API, 1);
    if (!OtherContextOrErr) {
      llvm::consumeError(OtherContextOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    DeviceContext &Context = *ContextOrErr;
    DeviceContext &OtherContext = *OtherContextOrErr;

    auto OtherModuleOrErr = OtherContext.loadModule(ImagePath);
    if (!OtherModuleOrErr) {
      llvm::consumeError(OtherModuleOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto OtherFunctionOrErr =
        OtherModuleOrErr->getFunction<ScaleFunction>("executionTestScale");
    if (!OtherFunctionOrErr) {
      llvm::consumeError(OtherFunctionOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    constexpr size_t ElementCount = 4;
    auto BufferOrErr = Context.createBuffer<int>(ElementCount);
    if (!BufferOrErr) {
      llvm::consumeError(BufferOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    LaunchConfig Config;
    Config.BlockDim.X = static_cast<uint32_t>(ElementCount);

    if (auto Err =
            Context.enqueueLaunch(*OtherFunctionOrErr, Config, *BufferOrErr,
                                  static_cast<unsigned int>(ElementCount), 7)) {
      llvm::consumeError(std::move(Err));
    } else
      MAGE_EXPECT_TRUE(false);
  });
}
