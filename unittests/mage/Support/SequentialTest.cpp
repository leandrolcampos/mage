//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests sequential execution of parallel utilities.
///
/// LLVM fixes the default executor's thread count on its first use, so these
/// tests use a separate executable from ParallelTest.cpp.
///
//===----------------------------------------------------------------------===//

#include "UnitTest/Test.hpp"
#include "mage/Support/Parallel.hpp"

#include "llvm/Support/Parallel.h"

#include <stddef.h>

using namespace mage;

namespace {

class SequentialTest : public mage::testing::Test {
public:
  void setUp() override { llvm::parallel::strategy.ThreadsRequested = 1; }
};

} // namespace

MAGE_TEST_F(SequentialTest, DoesNotRunZeroItems) {
  bool Called = false;
  parallelize(0, [&](size_t) { Called = true; });

  MAGE_EXPECT_FALSE(Called);
}

MAGE_TEST_F(SequentialTest, RunsEachItemOnce) {
  constexpr size_t NumWorkItems = 257;

  unsigned Counts[NumWorkItems] = {};
  size_t TotalCount = 0;

  const size_t NumThreads = getThreadCount();
  MAGE_ASSERT_EQ(NumThreads, size_t(1));

  parallelize(NumWorkItems, [&](size_t ItemIndex) {
    ++Counts[ItemIndex];
    ++TotalCount;

    MAGE_EXPECT_EQ(getThreadIndex(), 0U);
  });

  MAGE_EXPECT_EQ(TotalCount, NumWorkItems);

  size_t ExpectedCountPerItem = 1;
  for (size_t ItemIndex = 0; ItemIndex < NumWorkItems; ++ItemIndex)
    MAGE_EXPECT_EQ(Counts[ItemIndex], ExpectedCountPerItem);
}
