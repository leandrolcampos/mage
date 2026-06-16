//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements rounding-breakpoint distance calculations.
///
//===----------------------------------------------------------------------===//

#include "BreakpointDistance.hpp"

#include "WorstCases.hpp"

#include "mage/Testing/MpfrFloat.hpp"

#include <mpfr.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

using namespace mage;

using mage::worst_cases::RoundingGroupTy;

static void setFloatOverflowNeighbor(mpfr::MpfrFloat &Boundary, int Sign) {
  constexpr mpfr_exp_t OverflowExponent =
      static_cast<mpfr_exp_t>(std::numeric_limits<float>::max_exponent);

  mpfr_set_ui_2exp(*Boundary, 1, OverflowExponent, MPFR_RNDN);
  if (Sign < 0)
    mpfr_neg(*Boundary, *Boundary, MPFR_RNDN);
}

static void computeNearestFloats(mpfr::MpfrFloat &Lower, mpfr::MpfrFloat &Upper,
                                 const mpfr::MpfrFloat &FunctionValue,
                                 RoundingGroupTy RoundingGroup) {
  float LowerFloat = mpfr_get_flt(*FunctionValue, MPFR_RNDD);
  float UpperFloat = mpfr_get_flt(*FunctionValue, MPFR_RNDU);
  const bool HasLowerFloat = std::isfinite(LowerFloat);
  const bool HasUpperFloat = std::isfinite(UpperFloat);

  assert((HasLowerFloat || HasUpperFloat) &&
         "finite MPFR value must have a finite directed float bound");

  if (HasLowerFloat && HasUpperFloat) {
    if (LowerFloat == UpperFloat) {
      if (mpfr_sgn(*FunctionValue) <= 0)
        UpperFloat =
            std::nextafter(UpperFloat, std::numeric_limits<float>::infinity());
      else
        LowerFloat = std::nextafter(LowerFloat, 0.0f);
    }

    Lower.set(LowerFloat);
    Upper.set(UpperFloat);
  } else if (!HasLowerFloat) {
    if (RoundingGroup == RoundingGroupTy::Nearest) {
      setFloatOverflowNeighbor(Lower, -1);
      Upper.set(UpperFloat);
    } else {
      Lower.set(UpperFloat);
      Upper.set(std::nextafter(UpperFloat, 0.0f));
    }
  } else {
    if (RoundingGroup == RoundingGroupTy::Nearest) {
      Lower.set(LowerFloat);
      setFloatOverflowNeighbor(Upper, 1);
    } else {
      Lower.set(std::nextafter(LowerFloat, 0.0f));
      Upper.set(LowerFloat);
    }
  }

  assert(mpfr_less_p(*Lower, *Upper) != 0 &&
         "nearest floats must be strictly ordered");
}

static void computeUlp(mpfr::MpfrFloat &Ulp,
                       const mpfr::MpfrFloat &FunctionValue) {
  constexpr mpfr_exp_t MinimumSubnormalExponent =
      static_cast<mpfr_exp_t>(std::numeric_limits<float>::min_exponent -
                              std::numeric_limits<float>::digits);
  constexpr unsigned Precision = std::numeric_limits<float>::digits;

  if (mpfr_zero_p(*FunctionValue) != 0) {
    mpfr_set_ui_2exp(*Ulp, 1, MinimumSubnormalExponent, MPFR_RNDN);
    return;
  }

  const mpfr_exp_t Exponent = mpfr_get_exp(*FunctionValue);
  const mpfr_exp_t UlpExponent = std::max(
      Exponent - static_cast<mpfr_exp_t>(Precision), MinimumSubnormalExponent);
  mpfr_set_ui_2exp(*Ulp, 1, UlpExponent, MPFR_RNDN);
}

static void computeUlpDistance(mpfr::MpfrFloat &Distance,
                               const mpfr::MpfrFloat &Breakpoint,
                               const mpfr::MpfrFloat &FunctionValue) {
  mpfr::withInlineMpfrFloats<2>(
      Distance.getPrecision(), RoundingMode::NearestTiesToEven,
      [&](mpfr::MpfrFloat &Difference, mpfr::MpfrFloat &Ulp) {
        mpfr_sub(*Difference, *FunctionValue, *Breakpoint, MPFR_RNDA);
        mpfr_abs(*Difference, *Difference, MPFR_RNDA);
        computeUlp(Ulp, FunctionValue);
        mpfr_div(*Distance, *Difference, *Ulp, MPFR_RNDA);
      });
}

static void computeDirectedDistance(mpfr::MpfrFloat &Distance,
                                    const mpfr::MpfrFloat &FunctionValue) {
  mpfr::withInlineMpfrFloats<4>(
      Distance.getPrecision(), RoundingMode::NearestTiesToEven,
      [&](mpfr::MpfrFloat &Lower, mpfr::MpfrFloat &Upper,
          mpfr::MpfrFloat &LowerDifference, mpfr::MpfrFloat &UpperDifference) {
        computeNearestFloats(Lower, Upper, FunctionValue,
                             RoundingGroupTy::Directed);

        mpfr_sub(*LowerDifference, *FunctionValue, *Lower, MPFR_RNDA);
        mpfr_abs(*LowerDifference, *LowerDifference, MPFR_RNDA);
        mpfr_sub(*UpperDifference, *Upper, *FunctionValue, MPFR_RNDA);
        mpfr_abs(*UpperDifference, *UpperDifference, MPFR_RNDA);

        if (mpfr_less_p(*UpperDifference, *LowerDifference) != 0)
          computeUlpDistance(Distance, Upper, FunctionValue);
        else
          computeUlpDistance(Distance, Lower, FunctionValue);
      });
}

static void computeNearestDistance(mpfr::MpfrFloat &Distance,
                                   const mpfr::MpfrFloat &FunctionValue) {
  mpfr::withInlineMpfrFloats<3>(
      Distance.getPrecision(), RoundingMode::NearestTiesToEven,
      [&](mpfr::MpfrFloat &Lower, mpfr::MpfrFloat &Upper,
          mpfr::MpfrFloat &Breakpoint) {
        computeNearestFloats(Lower, Upper, FunctionValue,
                             RoundingGroupTy::Nearest);

        mpfr_sub(*Breakpoint, *Upper, *Lower, MPFR_RNDN);
        mpfr_div_2ui(*Breakpoint, *Breakpoint, 1, MPFR_RNDN);
        mpfr_add(*Breakpoint, *Lower, *Breakpoint, MPFR_RNDN);
        computeUlpDistance(Distance, Breakpoint, FunctionValue);
      });
}

bool mage::worst_cases::computeBreakpointDistance(
    mpfr::MpfrFloat &Distance, const mpfr::MpfrFloat &FunctionValue,
    RoundingGroupTy RoundingGroup) {
  if (mpfr_number_p(*FunctionValue) == 0)
    return false;

  if (RoundingGroup == RoundingGroupTy::Nearest)
    computeNearestDistance(Distance, FunctionValue);
  else
    computeDirectedDistance(Distance, FunctionValue);

  return true;
}
