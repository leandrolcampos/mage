//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests parallel execution utilities.
///
//===----------------------------------------------------------------------===//

#include "mage/Support/Parallel.hpp"
#include "UnitTest/Test.hpp"

#include "llvm/Support/Parallel.h"

#include <atomic>
#include <stddef.h>

using namespace mage;

MAGE_TEST(ParallelTest, DoesNotRunZeroItems) {
  bool Called = false;
  parallelize(0, [&](size_t) { Called = true; });

  MAGE_EXPECT_FALSE(Called);
}

MAGE_TEST(ParallelTest, RunsEachItemOnce) {
  constexpr size_t NumWorkItems = 257;

  MAGE_EXPECT_EQ(llvm::parallel::strategy.ThreadsRequested, 0U);

  std::atomic<unsigned> Counts[NumWorkItems] = {};
  std::atomic<size_t> InvalidThreadIndexCount = 0;
  std::atomic<size_t> TotalCount = 0;

  const size_t NumThreads = getThreadCount();
  MAGE_ASSERT_GE(NumThreads, size_t(1));

  parallelize(NumWorkItems, [&](size_t ItemIndex) {
    Counts[ItemIndex].fetch_add(1, std::memory_order_relaxed);
    TotalCount.fetch_add(1, std::memory_order_relaxed);

    if (getThreadIndex() >= NumThreads)
      InvalidThreadIndexCount.fetch_add(1, std::memory_order_relaxed);
  });

  MAGE_EXPECT_EQ(TotalCount.load(std::memory_order_relaxed), NumWorkItems);

  size_t ExpectedInvalidThreadIndexCount = 0;
  MAGE_EXPECT_EQ(InvalidThreadIndexCount.load(std::memory_order_relaxed),
                 ExpectedInvalidThreadIndexCount);

  size_t ExpectedCountPerItem = 1;
  for (size_t ItemIndex = 0; ItemIndex < NumWorkItems; ++ItemIndex)
    MAGE_EXPECT_EQ(Counts[ItemIndex].load(std::memory_order_relaxed),
                   ExpectedCountPerItem);
}
