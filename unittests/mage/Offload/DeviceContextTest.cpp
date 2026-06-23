//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests DeviceContext and related device-query APIs.
///
//===----------------------------------------------------------------------===//

#include "mage/Offload/DeviceContext.hpp"
#include "UnitTest/Test.hpp"

#include "llvm/Support/Error.h"

using namespace mage;

MAGE_TEST(DeviceContextTest, ConvertsDeviceAPIToString) {
  MAGE_EXPECT_STREQ(toString(DeviceAPI::CUDA), "CUDA");
  MAGE_EXPECT_STREQ(toString(DeviceAPI::HIP), "HIP");
}

MAGE_TEST(DeviceContextTest, GetsDeviceCountForSupportedAPIs) {
  auto ExpectDeviceCountIsNonNegativeOrUnavailable = [&](DeviceAPI API) {
    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    MAGE_EXPECT_GE(*CountOrErr, 0);
  };

  ExpectDeviceCountIsNonNegativeOrUnavailable(DeviceAPI::CUDA);
  ExpectDeviceCountIsNonNegativeOrUnavailable(DeviceAPI::HIP);
}

MAGE_TEST(DeviceContextTest, RejectsInvalidDeviceIDs) {
  auto ExpectInvalidDeviceIDIsRejected = [&](DeviceAPI API) {
    auto NegativeContextOrErr = DeviceContext::create(API, -1);
    MAGE_EXPECT_FALSE(static_cast<bool>(NegativeContextOrErr));
    if (!NegativeContextOrErr)
      llvm::consumeError(NegativeContextOrErr.takeError());

    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    auto PastEndContextOrErr = DeviceContext::create(API, *CountOrErr);
    MAGE_EXPECT_FALSE(static_cast<bool>(PastEndContextOrErr));
    if (!PastEndContextOrErr)
      llvm::consumeError(PastEndContextOrErr.takeError());
  };

  ExpectInvalidDeviceIDIsRejected(DeviceAPI::CUDA);
  ExpectInvalidDeviceIDIsRejected(DeviceAPI::HIP);
}

MAGE_TEST(DeviceContextTest, GetsNonEmptyDeviceNames) {
  auto ExpectDeviceNamesAreNonEmpty = [&](DeviceAPI API) {
    auto CountOrErr = getDeviceCount(API);
    if (!CountOrErr) {
      llvm::consumeError(CountOrErr.takeError());
      return;
    }

    for (int DeviceID = 0; DeviceID < *CountOrErr; ++DeviceID) {
      auto ContextOrErr = DeviceContext::create(API, DeviceID);
      if (!ContextOrErr) {
        llvm::consumeError(ContextOrErr.takeError());
        MAGE_EXPECT_TRUE(false);
        continue;
      }

      auto NameOrErr = ContextOrErr->getName();
      if (!NameOrErr) {
        llvm::consumeError(NameOrErr.takeError());
        MAGE_EXPECT_TRUE(false);
        continue;
      }

      MAGE_EXPECT_FALSE(NameOrErr->empty());
    }
  };

  ExpectDeviceNamesAreNonEmpty(DeviceAPI::CUDA);
  ExpectDeviceNamesAreNonEmpty(DeviceAPI::HIP);
}
