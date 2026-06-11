//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements parallel execution utilities.
///
//===----------------------------------------------------------------------===//

#include "mage/Support/Parallel.hpp"

#include "llvm/Support/Parallel.h"

#include <assert.h>

#include <algorithm>
#include <atomic>

using namespace mage;

size_t parallel::getThreadCount() { return llvm::parallel::getThreadCount(); }

unsigned parallel::getThreadIndex() { return llvm::parallel::getThreadIndex(); }

void parallel::detail::parallelize(size_t NumWorkItems,
                                   llvm::function_ref<void(size_t)> Fn) {
  assert(Fn && "Fn must be callable");

  if (NumWorkItems == 0)
    return;

  const size_t NumWorkers = std::min(NumWorkItems, parallel::getThreadCount());

  std::atomic<size_t> NextItemIndex = 0;
  auto Worker = [&] {
    while (true) {
      const size_t ItemIndex =
          NextItemIndex.fetch_add(1, std::memory_order_relaxed);
      if (ItemIndex >= NumWorkItems)
        break;
      Fn(ItemIndex);
    }
  };

  llvm::parallel::TaskGroup TG;

  for (size_t WorkerIndex = 0; WorkerIndex < NumWorkers; ++WorkerIndex)
    TG.spawn(Worker);
}
