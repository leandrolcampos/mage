//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares progress reporting for the worst-case search.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_TOOLS_MAGEWORSTCASES_PROGRESS_HPP
#define MAGE_TOOLS_MAGEWORSTCASES_PROGRESS_HPP

#include <atomic>
#include <stdint.h>
#include <thread>

namespace mage {
namespace worst_cases {

/// Reports aggregate progress from concurrent workers on a background thread.
class [[nodiscard]] ProgressReporter {
public:
  ProgressReporter(bool Enabled, uint64_t Total);
  ~ProgressReporter() noexcept;

  ProgressReporter(const ProgressReporter &) = delete;
  ProgressReporter &operator=(const ProgressReporter &) = delete;

  void add(uint64_t Amount) noexcept;
  void finish();

private:
  void reportProgress();

  bool Enabled;
  uint64_t Total;
  std::atomic<uint64_t> Processed = 0;
  std::atomic<bool> StopRequested = false;
  std::thread Thread;
};

} // namespace worst_cases
} // namespace mage

#endif // MAGE_TOOLS_MAGEWORSTCASES_PROGRESS_HPP
