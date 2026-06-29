//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests DeviceContext and related offload APIs.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/DeviceContext.hpp"
#include "UnitTest/Test.hpp"

#include "llvm/Support/Error.h"

#include <stddef.h>
#include <string>
#include <type_traits>
#include <utility>

using namespace mage;

namespace {
constexpr DeviceAPI DeviceAPIs[] = {DeviceAPI::CUDA, DeviceAPI::HIP};
} // namespace

template <typename Function> static void forEachDeviceAPI(Function &&Fn) {
  for (DeviceAPI API : DeviceAPIs)
    Fn(API);
}

MAGE_TEST(DeviceContextTest, ConvertsDeviceAPIToString) {
  MAGE_EXPECT_STREQ(toString(DeviceAPI::CUDA), "CUDA");
  MAGE_EXPECT_STREQ(toString(DeviceAPI::HIP), "HIP");
}

MAGE_TEST(DeviceContextTest, GetsDeviceCountForSupportedAPIs) {
  forEachDeviceAPI([&](DeviceAPI API) {
    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    MAGE_EXPECT_GE(*CountOrErr, 0);
  });
}

static_assert(!std::is_copy_constructible<DeviceContext>::value,
              "DeviceContext must not be copy constructible");
static_assert(!std::is_copy_assignable<DeviceContext>::value,
              "DeviceContext must not be copy assignable");
static_assert(std::is_move_constructible<DeviceContext>::value,
              "DeviceContext must be move constructible");
static_assert(std::is_move_assignable<DeviceContext>::value,
              "DeviceContext must be move assignable");

MAGE_TEST(DeviceContextTest, SupportsMoveConstruction) {
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

    DeviceContext MovedContext(std::move(*ContextOrErr));
    MAGE_EXPECT_EQ(MovedContext.getAPI(), API);
    MAGE_EXPECT_EQ(MovedContext.getID(), 0);
  });
}

MAGE_TEST(DeviceContextTest, SupportsMoveAssignment) {
  forEachDeviceAPI([&](DeviceAPI API) {
    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    if (*CountOrErr == 0)
      return;

    auto SourceOrErr = DeviceContext::create(API);
    if (!SourceOrErr) {
      llvm::consumeError(SourceOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto DestinationOrErr = DeviceContext::create(API);
    if (!DestinationOrErr) {
      llvm::consumeError(DestinationOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    *DestinationOrErr = std::move(*SourceOrErr);
    MAGE_EXPECT_EQ(DestinationOrErr->getAPI(), API);
    MAGE_EXPECT_EQ(DestinationOrErr->getID(), 0);
  });
}

MAGE_TEST(DeviceContextTest, CreatesDefaultContextForAvailableDevices) {
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

    MAGE_EXPECT_EQ(ContextOrErr->getAPI(), API);
    MAGE_EXPECT_EQ(ContextOrErr->getID(), 0);
  });
}

MAGE_TEST(DeviceContextTest, RejectsInvalidDeviceIDs) {
  forEachDeviceAPI([&](DeviceAPI API) {
    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    auto NegativeContextOrErr = DeviceContext::create(API, -1);
    if (!NegativeContextOrErr)
      llvm::consumeError(NegativeContextOrErr.takeError());
    else
      MAGE_EXPECT_TRUE(false);

    auto PastEndContextOrErr = DeviceContext::create(API, *CountOrErr);
    if (!PastEndContextOrErr)
      llvm::consumeError(PastEndContextOrErr.takeError());
    else
      MAGE_EXPECT_TRUE(false);
  });
}

MAGE_TEST(DeviceContextTest, GetsContextPropertiesForAvailableDevices) {
  forEachDeviceAPI([&](DeviceAPI API) {
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

      MAGE_EXPECT_EQ(ContextOrErr->getAPI(), API);
      MAGE_EXPECT_EQ(ContextOrErr->getID(), DeviceID);

      std::string Name = ContextOrErr->getName();
      std::string Architecture = ContextOrErr->getArchitecture();
      (void)Name;
      (void)Architecture;
    }
  });
}

MAGE_TEST(DeviceContextTest, GetsMemoryInfoForAvailableDevices) {
  forEachDeviceAPI([&](DeviceAPI API) {
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

      auto MemoryInfoOrErr = ContextOrErr->getMemoryInfo();
      if (!MemoryInfoOrErr) {
        llvm::consumeError(MemoryInfoOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      MAGE_EXPECT_LE(MemoryInfoOrErr->first, MemoryInfoOrErr->second);
      MAGE_EXPECT_GT(MemoryInfoOrErr->second, static_cast<size_t>(0));
    }
  });
}

MAGE_TEST(DeviceContextTest, SynchronizesAvailableContexts) {
  forEachDeviceAPI([&](DeviceAPI API) {
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

      if (auto Err = ContextOrErr->synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_EXPECT_TRUE(false);
      }
    }
  });
}
