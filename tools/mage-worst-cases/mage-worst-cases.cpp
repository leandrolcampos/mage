//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements the mage-worst-cases command-line interface.
///
//===----------------------------------------------------------------------===//

#include "WorstCases.hpp"

#include "mage/Testing/MpfrFloat.hpp"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"

#include <string>

using mage::worst_cases::findMathFunctionConfig;
using mage::worst_cases::OutputModeTy;
using mage::worst_cases::RoundingGroupTy;
using mage::worst_cases::SearchConfigTy;
using mage::worst_cases::searchWorstCases;

int main(int Argc, char **Argv) {
  llvm::InitLLVM InitLLVM(Argc, Argv);

  llvm::cl::OptionCategory WorstCasesCategory("mage-worst-cases options");

  llvm::cl::opt<std::string> FunctionName(
      llvm::cl::Positional, llvm::cl::desc("<math-function>"),
      llvm::cl::ValueRequired, llvm::cl::Required,
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<RoundingGroupTy> RoundingGroup(
      "rounding-group", llvm::cl::desc("target rounding mode group"),
      llvm::cl::values(clEnumValN(RoundingGroupTy::Nearest, "nearest",
                                  "target round-to-nearest modes"),
                       clEnumValN(RoundingGroupTy::Directed, "directed",
                                  "target directed rounding modes")),
      llvm::cl::init(RoundingGroupTy::Nearest),
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<OutputModeTy> OutputMode(
      "output-mode", llvm::cl::desc("output mode for the search results"),
      llvm::cl::values(
          clEnumValN(OutputModeTy::Count, "count",
                     "print only the number of worst cases found"),
          clEnumValN(OutputModeTy::Csv, "csv",
                     "write the worst cases and distances to a CSV file")),
      llvm::cl::init(OutputModeTy::Csv), llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<std::string> OutputDir(
      "output-dir", llvm::cl::desc("directory for CSV output files"),
      llvm::cl::value_desc("path"), llvm::cl::init("."),
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<std::string> DistanceBound(
      "distance-bound",
      llvm::cl::desc("maximum ULP distance from a rounding boundary"),
      llvm::cl::value_desc("number"), llvm::cl::init("0x1p-7"),
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<unsigned> MpfrPrecision(
      "mpfr-precision",
      llvm::cl::desc("bits of precision used for distance calculations"),
      llvm::cl::init(mage::mpfr::getExtraPrecision<float>()),
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<bool> NoProgress(
      "no-progress", llvm::cl::desc("disable the progress bar"),
      llvm::cl::init(false), llvm::cl::cat(WorstCasesCategory));

  llvm::cl::HideUnrelatedOptions(WorstCasesCategory);
  llvm::cl::ParseCommandLineOptions(
      Argc, Argv, "Search for float worst cases in elementary functions\n");

  const auto *FunctionConfig = findMathFunctionConfig(FunctionName);
  if (FunctionConfig == nullptr) {
    llvm::WithColor::error()
        << "unknown mathematical function '" << FunctionName << "'\n";
    return 1;
  }

  const SearchConfigTy SearchConfig = {
      FunctionConfig, RoundingGroup, OutputMode, MpfrPrecision,
      DistanceBound,  OutputDir,     !NoProgress};

  return searchWorstCases(SearchConfig) ? 0 : 1;
}
