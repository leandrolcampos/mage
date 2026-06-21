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
