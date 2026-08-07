//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares the worst-case search configuration and entry point.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_TOOLS_MAGEWORSTCASES_WORSTCASES_HPP
#define MAGE_TOOLS_MAGEWORSTCASES_WORSTCASES_HPP

#include "llvm/ADT/StringRef.h"

namespace mage {
namespace mpfr {
class MpfrFloat;
} // namespace mpfr

namespace worst_cases {

enum class OutputModeTy {
  Count,
  Csv,
};

enum class RoundingGroupTy {
  Nearest,
  Directed,
};

struct InputDomainTy {
  float Low;
  float High;
};

using MpfrFunctionTy = int (*)(mpfr::MpfrFloat &, const mpfr::MpfrFloat &);

struct FunctionConfigTy {
  llvm::StringRef Name;
  MpfrFunctionTy MpfrFunction;
  InputDomainTy InputDomain;
};

struct SearchConfigTy {
  const FunctionConfigTy *FunctionConfig;
  RoundingGroupTy RoundingGroup;
  OutputModeTy OutputMode;
  unsigned MpfrPrecision;
  llvm::StringRef DistanceBound;
  llvm::StringRef OutputDir;
  bool ShowProgress;
};

[[nodiscard]] const FunctionConfigTy *
findMathFunctionConfig(llvm::StringRef FunctionName) noexcept;

[[nodiscard]] bool searchWorstCases(const SearchConfigTy &SearchConfig);

} // namespace worst_cases
} // namespace mage

#endif // MAGE_TOOLS_MAGEWORSTCASES_WORSTCASES_HPP
