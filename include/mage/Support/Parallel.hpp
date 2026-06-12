//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares parallel execution utilities.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_SUPPORT_PARALLEL_HPP
#define MAGE_SUPPORT_PARALLEL_HPP

#include "llvm/ADT/STLFunctionalExtras.h"

#include <stddef.h>

namespace mage {
namespace detail {

void parallelize(size_t NumWorkItems, llvm::function_ref<void(size_t)> Fn);

} // namespace detail

/// Returns the number of threads available for parallel execution.
[[nodiscard]] size_t getThreadCount();

/// Returns the index of the thread executing the current work item.
///
/// This function must only be called from a function invoked by parallelize().
[[nodiscard]] unsigned getThreadIndex();

/// Invokes \p Fn for every index in [0, \p NumWorkItems), possibly in parallel,
/// and returns after all invocations complete.
///
/// \p Fn may be invoked concurrently and in an unspecified order.
template <typename Function>
void parallelize(size_t NumWorkItems, Function &&Fn) {
  detail::parallelize(NumWorkItems,
                      [&Fn](size_t WorkItemIndex) { Fn(WorkItemIndex); });
}

} // namespace mage

#endif // MAGE_SUPPORT_PARALLEL_HPP
