//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares rounding-breakpoint distance calculations.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_TOOLS_MAGEWORSTCASES_BREAKPOINTDISTANCE_HPP
#define MAGE_TOOLS_MAGEWORSTCASES_BREAKPOINTDISTANCE_HPP

#include "WorstCases.hpp"

namespace mage {
namespace worst_cases {

/// Computes the ULP distance from \p FunctionValue to the nearest rounding
/// breakpoint in \p RoundingGroup.
///
/// \param [out] Distance Receives the computed distance on success. It remains
/// unchanged if \p FunctionValue is NaN or infinite.
///
/// \returns True if \p FunctionValue is finite and the distance was computed;
/// false otherwise.
[[nodiscard]] bool
computeBreakpointDistance(mpfr::MpfrFloat &Distance,
                          const mpfr::MpfrFloat &FunctionValue,
                          RoundingGroupTy RoundingGroup) noexcept;

} // namespace worst_cases
} // namespace mage

#endif // MAGE_TOOLS_MAGEWORSTCASES_BREAKPOINTDISTANCE_HPP
