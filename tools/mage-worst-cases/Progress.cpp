//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements progress reporting for the worst-case search.
///
//===----------------------------------------------------------------------===//

#include "Progress.hpp"

#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>

static void printProgress(uint64_t Processed, uint64_t Total) {
  constexpr unsigned ProgressBarWidth = 40;

  const double Percentage =
      static_cast<double>(Processed) * 100.0 / static_cast<double>(Total);
  const unsigned CompletedWidth =
      std::min(ProgressBarWidth,
               static_cast<unsigned>(Percentage * ProgressBarWidth / 100.0));

  llvm::outs() << "\r[";
  for (unsigned I = 0; I < ProgressBarWidth; ++I)
    llvm::outs() << (I < CompletedWidth ? '=' : ' ');
  llvm::outs() << "] " << llvm::format("%6.2f%%", Percentage);
  llvm::outs().flush();
}

namespace mage {
namespace worst_cases {

ProgressReporter::ProgressReporter(bool Enabled, uint64_t Total)
    : Enabled(Enabled), Total(Total) {
  assert((Total > 0) && "total must be greater than zero");

  if (Enabled)
    Thread = std::thread([this] { reportProgress(); });
}

ProgressReporter::~ProgressReporter() noexcept { finish(); }

void ProgressReporter::add(uint64_t Amount) noexcept {
  if (Enabled)
    Processed.fetch_add(Amount, std::memory_order_relaxed);
}

void ProgressReporter::finish() {
  if (!Thread.joinable())
    return;

  StopRequested.store(true, std::memory_order_release);
  Thread.join();
}

void ProgressReporter::reportProgress() {
  uint64_t LastProcessed = std::numeric_limits<uint64_t>::max();

  while (!StopRequested.load(std::memory_order_acquire)) {
    const uint64_t Current = Processed.load(std::memory_order_relaxed);
    if (Current != LastProcessed) {
      printProgress(Current, Total);
      LastProcessed = Current;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  const uint64_t Current = Processed.load(std::memory_order_relaxed);
  if (Current != LastProcessed)
    printProgress(Current, Total);
  llvm::outs() << '\n';
}

} // namespace worst_cases
} // namespace mage
