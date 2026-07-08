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

#include "mage/Offload/Memory.hpp"
#include "UnitTest/Test.hpp"

#include "mage/Offload/Context.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"

#include <limits>
#include <stddef.h>
#include <type_traits>
#include <utility>

using namespace mage;

namespace {
constexpr DeviceAPI DeviceAPIs[] = {DeviceAPI::CUDA, DeviceAPI::HIP};
constexpr size_t DeviceAPICount = sizeof(DeviceAPIs) / sizeof(DeviceAPIs[0]);
} // namespace

template <typename Func> static void forEachDeviceAPI(Func &&Fn) {
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

MAGE_TEST(MemoryTest, SupportsEmptyHostBuffers) {
  HostBuffer<int> Buffer;

  MAGE_EXPECT_TRUE(Buffer.empty());
  MAGE_EXPECT_EQ(Buffer.size(), static_cast<size_t>(0));
  MAGE_EXPECT_TRUE(Buffer.data() == nullptr);

  llvm::ArrayRef<int> Values = Buffer;
  MAGE_EXPECT_TRUE(Values.empty());

  llvm::MutableArrayRef<int> MutableValues = Buffer;
  MAGE_EXPECT_TRUE(MutableValues.empty());
}

MAGE_TEST(MemoryTest, CreatesHostBuffersForAvailableDevices) {
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

MAGE_TEST(MemoryTest, CreatesEmptyHostBuffersForAvailableDevices) {
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

      auto BufferOrErr = ContextOrErr->createHostBuffer<int>(0);
      if (!BufferOrErr) {
        llvm::consumeError(BufferOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &Buffer = *BufferOrErr;
      MAGE_EXPECT_TRUE(Buffer.empty());
      MAGE_EXPECT_EQ(Buffer.size(), static_cast<size_t>(0));
      MAGE_EXPECT_TRUE(Buffer.data() == nullptr);
    }
  });
}

MAGE_TEST(MemoryTest, RejectsHostBufferAllocationSizeOverflows) {
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

    constexpr size_t TooLargeElementCount =
        std::numeric_limits<size_t>::max() / sizeof(int) + 1;
    auto BufferOrErr =
        ContextOrErr->createHostBuffer<int>(TooLargeElementCount);
    if (!BufferOrErr)
      llvm::consumeError(BufferOrErr.takeError());
    else
      MAGE_EXPECT_TRUE(false);
  });
}

MAGE_TEST(MemoryTest, MovesHostBuffersForAvailableDevices) {
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
      HostBuffer<int> MoveConstructed(std::move(Source));

      MAGE_EXPECT_TRUE(Source.empty());
      MAGE_EXPECT_EQ(Source.size(), static_cast<size_t>(0));
      MAGE_EXPECT_TRUE(Source.data() == nullptr);

      MAGE_EXPECT_EQ(MoveConstructed.size(), static_cast<size_t>(4));
      MAGE_EXPECT_FALSE(MoveConstructed.empty());
      MAGE_EXPECT_TRUE(MoveConstructed.data() != nullptr);

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
      MAGE_EXPECT_FALSE(Destination.empty());
      MAGE_EXPECT_TRUE(Destination.data() != nullptr);
    }
  });
}

MAGE_TEST(MemoryTest, ConvertsHostBuffersToArrayRefsForAvailableDevices) {
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

static_assert(!std::is_copy_constructible<DeviceBuffer<int>>::value,
              "DeviceBuffer must not be copy constructible");
static_assert(!std::is_copy_assignable<DeviceBuffer<int>>::value,
              "DeviceBuffer must not be copy assignable");
static_assert(std::is_move_constructible<DeviceBuffer<int>>::value,
              "DeviceBuffer must be move constructible");
static_assert(std::is_move_assignable<DeviceBuffer<int>>::value,
              "DeviceBuffer must be move assignable");

MAGE_TEST(MemoryTest, SupportsEmptyBuffers) {
  DeviceBuffer<int> Buffer;

  MAGE_EXPECT_TRUE(Buffer.empty());
  MAGE_EXPECT_EQ(Buffer.size(), static_cast<size_t>(0));
  MAGE_EXPECT_TRUE(Buffer.data() == nullptr);
}

MAGE_TEST(MemoryTest, CreatesBuffersForAvailableDevices) {
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

      auto BufferOrErr = ContextOrErr->createBuffer<int>(4);
      if (!BufferOrErr) {
        llvm::consumeError(BufferOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceBuffer<int> &Buffer = *BufferOrErr;
      MAGE_EXPECT_EQ(Buffer.size(), static_cast<size_t>(4));
      MAGE_EXPECT_FALSE(Buffer.empty());
      MAGE_EXPECT_TRUE(Buffer.data() != nullptr);

      if (auto Err = ContextOrErr->synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_EXPECT_TRUE(false);
      }
    }
  });
}

MAGE_TEST(MemoryTest, CreatesEmptyBuffersForAvailableDevices) {
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

      auto BufferOrErr = ContextOrErr->createBuffer<int>(0);
      if (!BufferOrErr) {
        llvm::consumeError(BufferOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceBuffer<int> &Buffer = *BufferOrErr;
      MAGE_EXPECT_TRUE(Buffer.empty());
      MAGE_EXPECT_EQ(Buffer.size(), static_cast<size_t>(0));
      MAGE_EXPECT_TRUE(Buffer.data() == nullptr);
    }
  });
}

MAGE_TEST(MemoryTest, RejectsBufferAllocationSizeOverflows) {
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

    constexpr size_t TooLargeElementCount =
        std::numeric_limits<size_t>::max() / sizeof(int) + 1;
    auto BufferOrErr = ContextOrErr->createBuffer<int>(TooLargeElementCount);
    if (!BufferOrErr)
      llvm::consumeError(BufferOrErr.takeError());
    else
      MAGE_EXPECT_TRUE(false);
  });
}

MAGE_TEST(MemoryTest, MovesBuffersForAvailableDevices) {
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

      auto SourceOrErr = ContextOrErr->createBuffer<int>(4);
      if (!SourceOrErr) {
        llvm::consumeError(SourceOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceBuffer<int> &Source = *SourceOrErr;
      DeviceBuffer<int> MoveConstructed(std::move(Source));

      MAGE_EXPECT_TRUE(Source.empty());
      MAGE_EXPECT_EQ(Source.size(), static_cast<size_t>(0));
      MAGE_EXPECT_TRUE(Source.data() == nullptr);

      MAGE_EXPECT_EQ(MoveConstructed.size(), static_cast<size_t>(4));
      MAGE_EXPECT_FALSE(MoveConstructed.empty());
      MAGE_EXPECT_TRUE(MoveConstructed.data() != nullptr);

      auto DestinationOrErr = ContextOrErr->createBuffer<int>(2);
      if (!DestinationOrErr) {
        llvm::consumeError(DestinationOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceBuffer<int> &Destination = *DestinationOrErr;
      Destination = std::move(MoveConstructed);

      MAGE_EXPECT_TRUE(MoveConstructed.empty());
      MAGE_EXPECT_EQ(MoveConstructed.size(), static_cast<size_t>(0));
      MAGE_EXPECT_TRUE(MoveConstructed.data() == nullptr);

      MAGE_EXPECT_EQ(Destination.size(), static_cast<size_t>(4));
      MAGE_EXPECT_FALSE(Destination.empty());
      MAGE_EXPECT_TRUE(Destination.data() != nullptr);

      if (auto Err = ContextOrErr->synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_EXPECT_TRUE(false);
      }
    }
  });
}

MAGE_TEST(MemoryTest, CopiesToEmptyDestinationBuffers) {
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

    DeviceBuffer<int> EmptyDeviceDestination;
    HostBuffer<int> EmptyHostSource;
    if (auto Err = ContextOrErr->enqueueCopy(EmptyDeviceDestination,
                                             EmptyHostSource)) {
      llvm::consumeError(std::move(Err));
      MAGE_EXPECT_TRUE(false);
    }

    HostBuffer<int> EmptyHostDestination;
    DeviceBuffer<int> EmptyDeviceSource;
    if (auto Err = ContextOrErr->enqueueCopy(EmptyHostDestination,
                                             EmptyDeviceSource)) {
      llvm::consumeError(std::move(Err));
      MAGE_EXPECT_TRUE(false);
    }
  });
}

MAGE_TEST(MemoryTest, CopiesBuffersForAvailableDevices) {
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

      auto IntermediateOrErr = ContextOrErr->createBuffer<int>(4);
      if (!IntermediateOrErr) {
        llvm::consumeError(IntermediateOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      DeviceBuffer<int> &Intermediate = *IntermediateOrErr;
      const HostBuffer<int> &ConstSource = Source;
      if (auto Err = ContextOrErr->enqueueCopy(Intermediate, ConstSource)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      auto DestinationOrErr = ContextOrErr->createHostBuffer<int>(4);
      if (!DestinationOrErr) {
        llvm::consumeError(DestinationOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &Destination = *DestinationOrErr;
      const DeviceBuffer<int> &ConstIntermediate = Intermediate;
      if (auto Err =
              ContextOrErr->enqueueCopy(Destination, ConstIntermediate)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = ContextOrErr->synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      MAGE_EXPECT_EQ(Destination[0], 3);
      MAGE_EXPECT_EQ(Destination[1], 1);
      MAGE_EXPECT_EQ(Destination[2], 4);
      MAGE_EXPECT_EQ(Destination[3], 2);
    }
  });
}

MAGE_TEST(MemoryTest, RejectsCopiesFromTooSmallSources) {
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

    auto SmallHostOrErr = ContextOrErr->createHostBuffer<int>(2);
    if (!SmallHostOrErr) {
      llvm::consumeError(SmallHostOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto LargeDeviceOrErr = ContextOrErr->createBuffer<int>(4);
    if (!LargeDeviceOrErr) {
      llvm::consumeError(LargeDeviceOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    if (auto Err =
            ContextOrErr->enqueueCopy(*LargeDeviceOrErr, *SmallHostOrErr))
      llvm::consumeError(std::move(Err));
    else
      MAGE_EXPECT_TRUE(false);

    auto LargeHostOrErr = ContextOrErr->createHostBuffer<int>(4);
    if (!LargeHostOrErr) {
      llvm::consumeError(LargeHostOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto SmallDeviceOrErr = ContextOrErr->createBuffer<int>(2);
    if (!SmallDeviceOrErr) {
      llvm::consumeError(SmallDeviceOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    if (auto Err =
            ContextOrErr->enqueueCopy(*LargeHostOrErr, *SmallDeviceOrErr))
      llvm::consumeError(std::move(Err));
    else
      MAGE_EXPECT_TRUE(false);
  });
}

MAGE_TEST(MemoryTest, CopiesBuffersCreatedByOtherContextsOnSameDevice) {
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

      auto OtherContextOrErr = DeviceContext::create(API, DeviceID);
      if (!OtherContextOrErr) {
        llvm::consumeError(OtherContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto HostSourceOrErr = ContextOrErr->createHostBuffer<int>(4);
      if (!HostSourceOrErr) {
        llvm::consumeError(HostSourceOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &HostSource = *HostSourceOrErr;
      HostSource[0] = 3;
      HostSource[1] = 1;
      HostSource[2] = 4;
      HostSource[3] = 2;

      auto DeviceBufferOrErr = OtherContextOrErr->createBuffer<int>(4);
      if (!DeviceBufferOrErr) {
        llvm::consumeError(DeviceBufferOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err =
              ContextOrErr->enqueueCopy(*DeviceBufferOrErr, *HostSourceOrErr)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      auto HostDestinationOrErr = ContextOrErr->createHostBuffer<int>(4);
      if (!HostDestinationOrErr) {
        llvm::consumeError(HostDestinationOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = ContextOrErr->enqueueCopy(*HostDestinationOrErr,
                                               *DeviceBufferOrErr)) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = ContextOrErr->synchronize()) {
        llvm::consumeError(std::move(Err));
        MAGE_ASSERT_TRUE(false);
      }

      HostBuffer<int> &HostDestination = *HostDestinationOrErr;
      MAGE_EXPECT_EQ(HostDestination[0], 3);
      MAGE_EXPECT_EQ(HostDestination[1], 1);
      MAGE_EXPECT_EQ(HostDestination[2], 4);
      MAGE_EXPECT_EQ(HostDestination[3], 2);
    }
  });
}

MAGE_TEST(MemoryTest, RejectsCopiesWithBuffersFromOtherDevices) {
  forEachDeviceAPI([&](DeviceAPI API) {
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

    auto HostSourceOrErr = ContextOrErr->createHostBuffer<int>(4);
    if (!HostSourceOrErr) {
      llvm::consumeError(HostSourceOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    auto ForeignDeviceOrErr = OtherContextOrErr->createBuffer<int>(4);
    if (!ForeignDeviceOrErr) {
      llvm::consumeError(ForeignDeviceOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    if (auto Err =
            ContextOrErr->enqueueCopy(*ForeignDeviceOrErr, *HostSourceOrErr))
      llvm::consumeError(std::move(Err));
    else
      MAGE_EXPECT_TRUE(false);

    auto HostDestinationOrErr = ContextOrErr->createHostBuffer<int>(4);
    if (!HostDestinationOrErr) {
      llvm::consumeError(HostDestinationOrErr.takeError());
      MAGE_ASSERT_TRUE(false);
    }

    if (auto Err = ContextOrErr->enqueueCopy(*HostDestinationOrErr,
                                             *ForeignDeviceOrErr))
      llvm::consumeError(std::move(Err));
    else
      MAGE_EXPECT_TRUE(false);
  });
}

MAGE_TEST(MemoryTest, RejectsCopiesWithHostBuffersFromOtherAPIs) {
  for (size_t I = 0; I < DeviceAPICount; ++I) {
    DeviceAPI API = DeviceAPIs[I];

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      continue;
    }

    if (*CountOrErr == 0)
      continue;

    for (size_t J = I + 1; J < DeviceAPICount; ++J) {
      DeviceAPI OtherAPI = DeviceAPIs[J];

      auto OtherCountOrErr = getDeviceCount(OtherAPI);
      if (!OtherCountOrErr) {
        llvm::consumeError(OtherCountOrErr.takeError());
        continue;
      }

      if (*OtherCountOrErr == 0)
        continue;

      auto ContextOrErr = DeviceContext::create(API);
      if (!ContextOrErr) {
        llvm::consumeError(ContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto OtherContextOrErr = DeviceContext::create(OtherAPI);
      if (!OtherContextOrErr) {
        llvm::consumeError(OtherContextOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto DeviceBufferOrErr = ContextOrErr->createBuffer<int>(4);
      if (!DeviceBufferOrErr) {
        llvm::consumeError(DeviceBufferOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      auto ForeignHostSourceOrErr = OtherContextOrErr->createHostBuffer<int>(4);
      if (!ForeignHostSourceOrErr) {
        llvm::consumeError(ForeignHostSourceOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = ContextOrErr->enqueueCopy(*DeviceBufferOrErr,
                                               *ForeignHostSourceOrErr))
        llvm::consumeError(std::move(Err));
      else
        MAGE_EXPECT_TRUE(false);

      auto ForeignHostDestinationOrErr =
          OtherContextOrErr->createHostBuffer<int>(4);
      if (!ForeignHostDestinationOrErr) {
        llvm::consumeError(ForeignHostDestinationOrErr.takeError());
        MAGE_ASSERT_TRUE(false);
      }

      if (auto Err = ContextOrErr->enqueueCopy(*ForeignHostDestinationOrErr,
                                               *DeviceBufferOrErr))
        llvm::consumeError(std::move(Err));
      else
        MAGE_EXPECT_TRUE(false);

      return;
    }
  }
}
