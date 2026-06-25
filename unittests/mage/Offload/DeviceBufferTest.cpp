//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests typed host and device buffers used by offload operations.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/DeviceBuffer.hpp"
#include "UnitTest/Test.hpp"

#include "mage/Offload/DeviceContext.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"

#include <stddef.h>
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

static_assert(!std::is_copy_constructible<HostBuffer<int>>::value,
              "HostBuffer must not be copy constructible");
static_assert(!std::is_copy_assignable<HostBuffer<int>>::value,
              "HostBuffer must not be copy assignable");
static_assert(std::is_move_constructible<HostBuffer<int>>::value,
              "HostBuffer must be move constructible");
static_assert(std::is_move_assignable<HostBuffer<int>>::value,
              "HostBuffer must be move assignable");
static_assert(
    std::is_convertible<HostBuffer<int> &, llvm::ArrayRef<int>>::value,
    "HostBuffer must convert to ArrayRef");
static_assert(
    std::is_convertible<HostBuffer<int> &, llvm::MutableArrayRef<int>>::value,
    "HostBuffer must convert to MutableArrayRef");

MAGE_TEST(DeviceBufferTest, SupportsEmptyHostBuffers) {
  HostBuffer<int> Buffer;

  MAGE_EXPECT_TRUE(Buffer.empty());
  MAGE_EXPECT_EQ(Buffer.size(), static_cast<size_t>(0));
  MAGE_EXPECT_TRUE(Buffer.data() == nullptr);

  llvm::ArrayRef<int> Values = Buffer;
  MAGE_EXPECT_TRUE(Values.empty());

  llvm::MutableArrayRef<int> MutableValues = Buffer;
  MAGE_EXPECT_TRUE(MutableValues.empty());
}

MAGE_TEST(DeviceBufferTest, CreatesHostBuffersForAvailableDevices) {
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

      auto BufferOrErr = ContextOrErr->createHostBuffer<int>(4);
      if (!BufferOrErr) {
        llvm::consumeError(BufferOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &Buffer = *BufferOrErr;
      MAGE_EXPECT_EQ(Buffer.size(), static_cast<size_t>(4));
      MAGE_EXPECT_FALSE(Buffer.empty());
      MAGE_ASSERT_TRUE(Buffer.data() != nullptr);

      Buffer[0] = 3;
      Buffer[1] = 1;
      Buffer[2] = 4;
      Buffer[3] = 2;

      MAGE_EXPECT_EQ(Buffer[0], 3);
      MAGE_EXPECT_EQ(Buffer[1], 1);
      MAGE_EXPECT_EQ(Buffer[2], 4);
      MAGE_EXPECT_EQ(Buffer[3], 2);
    }
  });
}

MAGE_TEST(DeviceBufferTest, MovesHostBuffersForAvailableDevices) {
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

      auto SourceOrErr = ContextOrErr->createHostBuffer<int>(4);
      if (!SourceOrErr) {
        llvm::consumeError(SourceOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &Source = *SourceOrErr;
      Source[0] = 3;
      Source[1] = 1;
      Source[2] = 4;
      Source[3] = 2;

      HostBuffer<int> MoveConstructed(std::move(Source));
      MAGE_EXPECT_TRUE(Source.empty());
      MAGE_EXPECT_EQ(Source.size(), static_cast<size_t>(0));
      MAGE_EXPECT_TRUE(Source.data() == nullptr);

      MAGE_EXPECT_EQ(MoveConstructed.size(), static_cast<size_t>(4));
      MAGE_EXPECT_EQ(MoveConstructed[0], 3);
      MAGE_EXPECT_EQ(MoveConstructed[1], 1);
      MAGE_EXPECT_EQ(MoveConstructed[2], 4);
      MAGE_EXPECT_EQ(MoveConstructed[3], 2);

      auto DestinationOrErr = ContextOrErr->createHostBuffer<int>(2);
      if (!DestinationOrErr) {
        llvm::consumeError(DestinationOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &Destination = *DestinationOrErr;
      Destination = std::move(MoveConstructed);
      MAGE_EXPECT_TRUE(MoveConstructed.empty());
      MAGE_EXPECT_EQ(MoveConstructed.size(), static_cast<size_t>(0));
      MAGE_EXPECT_TRUE(MoveConstructed.data() == nullptr);

      MAGE_EXPECT_EQ(Destination.size(), static_cast<size_t>(4));
      MAGE_EXPECT_EQ(Destination[0], 3);
      MAGE_EXPECT_EQ(Destination[1], 1);
      MAGE_EXPECT_EQ(Destination[2], 4);
      MAGE_EXPECT_EQ(Destination[3], 2);
    }
  });
}

MAGE_TEST(DeviceBufferTest, ConvertsHostBuffersToArrayRefsForAvailableDevices) {
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

      auto BufferOrErr = ContextOrErr->createHostBuffer<int>(4);
      if (!BufferOrErr) {
        llvm::consumeError(BufferOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &Buffer = *BufferOrErr;

      llvm::MutableArrayRef<int> MutableValues = Buffer;
      MAGE_EXPECT_EQ(MutableValues.size(), static_cast<size_t>(4));
      MAGE_EXPECT_TRUE(MutableValues.data() == Buffer.data());

      MutableValues[0] = 3;
      MutableValues[1] = 1;
      MutableValues[2] = 4;
      MutableValues[3] = 2;

      llvm::ArrayRef<int> Values = Buffer;
      MAGE_EXPECT_EQ(Values.size(), static_cast<size_t>(4));
      MAGE_EXPECT_TRUE(Values.data() == Buffer.data());

      MAGE_EXPECT_EQ(Values[0], 3);
      MAGE_EXPECT_EQ(Values[1], 1);
      MAGE_EXPECT_EQ(Values[2], 4);
      MAGE_EXPECT_EQ(Values[3], 2);
    }
  });
}
