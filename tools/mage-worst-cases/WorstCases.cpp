//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements the parallel MPFR worst-case search.
///
//===----------------------------------------------------------------------===//

#include "WorstCases.hpp"

#include "BreakpointDistance.hpp"
#include "CsvOutput.hpp"
#include "Progress.hpp"

#include "mage/Support/Parallel.hpp"
#include "mage/Support/Range.hpp"
#include "mage/Testing/MpfrFloat.hpp"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include <mpfr.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

using namespace mage;

using mage::worst_cases::CsvOutput;
using mage::worst_cases::FunctionConfigTy;
using mage::worst_cases::OutputModeTy;
using mage::worst_cases::ProgressReporter;
using mage::worst_cases::RoundingGroupTy;
using mage::worst_cases::SearchConfigTy;

static constexpr FunctionConfigTy FunctionConfigs[] = {
    {"exp",
     mpfr::exp,
     {-std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}},
    {"log",
     mpfr::log,
     {std::numeric_limits<float>::denorm_min(),
      std::numeric_limits<float>::max()}},
};

static llvm::StringRef getRoundingGroupName(RoundingGroupTy RoundingGroup) {
  switch (RoundingGroup) {
  case RoundingGroupTy::Nearest:
    return "nearest";
  case RoundingGroupTy::Directed:
    return "directed";
  }
  // TODO: Use MAGE_UNREACHABLE once available.
  llvm_unreachable("unknown rounding group");
}

static std::string getOutputPath(const SearchConfigTy &SearchConfig) {
  llvm::SmallString<256> Path(SearchConfig.OutputDir);
  llvm::sys::path::append(
      Path, (SearchConfig.FunctionConfig->Name + "-" +
             getRoundingGroupName(SearchConfig.RoundingGroup) + ".csv")
                .str());
  return std::string(Path.str());
}

static size_t getNumPartitions(uint64_t InputCount, size_t AvailableThreads) {
  assert((InputCount > 0) && "input count must be greater than zero");

  // Require this many inputs before assigning another worker to the search.
  constexpr uint64_t GrainSize = 32768;

  const uint64_t PartitionsForInputCount = (InputCount - 1) / GrainSize + 1;

  return static_cast<size_t>(
      std::min<uint64_t>(AvailableThreads, PartitionsForInputCount));
}

static bool setDistanceBound(mpfr::MpfrFloat &DistanceBound,
                             llvm::StringRef Text) {
  const std::string Storage = Text.str();
  return mpfr_set_str(*DistanceBound, Storage.c_str(), 0, MPFR_RNDN) == 0;
}

[[nodiscard]] static bool
validateDistanceBound(const SearchConfigTy &SearchConfig) {
  mpfr::MpfrFloat DistanceBound(SearchConfig.MpfrPrecision,
                                RoundingMode::NearestTiesToEven);
  if (!setDistanceBound(DistanceBound, SearchConfig.DistanceBound)) {
    llvm::WithColor::error()
        << "invalid distance bound: " << SearchConfig.DistanceBound << '\n';
    return false;
  }

  if (mpfr_number_p(*DistanceBound) == 0) {
    llvm::WithColor::error() << "distance bound must be finite\n";
    return false;
  }

  if (mpfr_sgn(*DistanceBound) < 0) {
    llvm::WithColor::error() << "distance bound must be non-negative\n";
    return false;
  }

  return true;
}

[[nodiscard]] static uint64_t
searchPartition(const SearchConfigTy &SearchConfig,
                const range<float> &Partition, size_t PartitionIndex,
                CsvOutput *Output, ProgressReporter &Progress) {
  constexpr uint64_t ProgressUpdateInterval = uint64_t(1) << 20;

  uint64_t WorstCaseCount = 0;
  uint64_t PendingProgress = 0;
  const RoundingMode Rounding = RoundingMode::NearestTiesToEven;

  mpfr_exp_t PreviousMinimumExponent = mpfr_get_emin();
  mpfr_exp_t PreviousMaximumExponent = mpfr_get_emax();

  mpfr_set_emin(mpfr_get_emin_min());
  mpfr_set_emax(mpfr_get_emax_max());

  mpfr::MpfrFloat Input(SearchConfig.MpfrPrecision, Rounding);
  mpfr::MpfrFloat FunctionValue(SearchConfig.MpfrPrecision, Rounding);
  mpfr::MpfrFloat Distance(SearchConfig.MpfrPrecision, Rounding);
  mpfr::MpfrFloat DistanceBound(SearchConfig.MpfrPrecision, Rounding);

  setDistanceBound(DistanceBound, SearchConfig.DistanceBound);

  const auto MpfrFunction = SearchConfig.FunctionConfig->MpfrFunction;

  for (float Value : Partition) {
    ++PendingProgress;
    if (PendingProgress == ProgressUpdateInterval) {
      Progress.add(PendingProgress);
      PendingProgress = 0;
    }

    Input.set(Value);

    MpfrFunction(FunctionValue, Input);

    if (!computeBreakpointDistance(Distance, FunctionValue,
                                   SearchConfig.RoundingGroup))
      continue;

    if (mpfr_greater_p(*Distance, *DistanceBound) != 0)
      continue;

    ++WorstCaseCount;
    if (Output != nullptr) {
      const float RoundedDistance = mpfr_get_flt(*Distance, MPFR_RNDN);
      Output->write(PartitionIndex, Value, RoundedDistance);
    }
  }

  mpfr_set_emin(PreviousMinimumExponent);
  mpfr_set_emax(PreviousMaximumExponent);

  Progress.add(PendingProgress);
  return WorstCaseCount;
}

const FunctionConfigTy *mage::worst_cases::findMathFunctionConfig(
    llvm::StringRef FunctionName) noexcept {
  for (const FunctionConfigTy &FunctionConfig : FunctionConfigs)
    if (FunctionConfig.Name == FunctionName)
      return &FunctionConfig;
  return nullptr;
}

bool mage::worst_cases::searchWorstCases(const SearchConfigTy &SearchConfig) {
  constexpr unsigned MinimumMpfrPrecision =
      std::numeric_limits<float>::digits + 1;

  if (SearchConfig.MpfrPrecision < MinimumMpfrPrecision) {
    llvm::WithColor::error()
        << "precision must be at least " << MinimumMpfrPrecision << " bits\n";
    return false;
  }

  if (!validateDistanceBound(SearchConfig))
    return false;

  const range<float> Inputs(SearchConfig.FunctionConfig->InputDomain.Low,
                            SearchConfig.FunctionConfig->InputDomain.High);
  const size_t AvailableThreads = getThreadCount();
  const size_t NumPartitions =
      getNumPartitions(Inputs.size(), AvailableThreads);

  if (NumPartitions > 1 && mpfr_buildopt_tls_p() == 0) {
    llvm::WithColor::error()
        << "parallel search requires MPFR thread-local storage support\n";
    return false;
  }

  const auto StartTime = std::chrono::steady_clock::now();

  std::string OutputPath;
  std::unique_ptr<CsvOutput> Output;
  if (SearchConfig.OutputMode == OutputModeTy::Csv) {
    OutputPath = getOutputPath(SearchConfig);
    llvm::Expected<std::unique_ptr<CsvOutput>> OutputOrError =
        CsvOutput::create(OutputPath, NumPartitions);
    if (!OutputOrError) {
      llvm::logAllUnhandledErrors(OutputOrError.takeError(), llvm::errs(),
                                  "error: ");
      return false;
    }
    Output = std::move(*OutputOrError);
  }

  llvm::outs() << "Inputs: " << Inputs.size() << '\n'
               << "Available threads: " << AvailableThreads << '\n'
               << "Partitions: " << NumPartitions << '\n';
  if (SearchConfig.ShowProgress)
    llvm::outs() << "Progress:\n";
  llvm::outs().flush();

  std::vector<uint64_t> WorstCaseCounts(NumPartitions);
  ProgressReporter Progress(SearchConfig.ShowProgress, Inputs.size());

  parallelize(NumPartitions, [&](size_t PartitionIndex) {
    const range<float> Partition =
        Inputs.stridedPartition(PartitionIndex, NumPartitions);
    WorstCaseCounts[PartitionIndex] = searchPartition(
        SearchConfig, Partition, PartitionIndex, Output.get(), Progress);
  });

  Progress.finish();
  const uint64_t WorstCaseCount =
      llvm::accumulate(WorstCaseCounts, uint64_t(0));

  if (Output != nullptr) {
    if (llvm::Error Error = Output->finalize()) {
      llvm::logAllUnhandledErrors(std::move(Error), llvm::errs(), "error: ");
      return false;
    }
    llvm::outs() << "Wrote " << WorstCaseCount << " worst case(s) to "
                 << OutputPath << '\n';
  } else {
    llvm::outs() << "Worst case count: " << WorstCaseCount << '\n';
  }

  const auto EndTime = std::chrono::steady_clock::now();
  const std::chrono::duration<double> ElapsedTime = EndTime - StartTime;
  llvm::outs() << "Elapsed time: " << llvm::format("%.3f", ElapsedTime.count())
               << " seconds\n";

  return true;
}
