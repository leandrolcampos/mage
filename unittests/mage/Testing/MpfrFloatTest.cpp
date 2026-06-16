//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests MPFR-based floating-point testing utilities.
///
//===----------------------------------------------------------------------===//

#include "mage/Testing/MpfrFloat.hpp"
#include "UnitTest/Test.hpp"

#include "mage/Support/TypeTraits.hpp"

#include <stdint.h>
#include <utility>

using namespace mage;

static_assert(mpfr::getExtraPrecision<float16>() == 128);
static_assert(mpfr::getExtraPrecision<float>() == 128);
static_assert(mpfr::getExtraPrecision<const double>() == 256);

static_assert(
    is_same_v<decltype(*std::declval<mpfr::MpfrFloat &>()), mpfr_ptr>);
static_assert(
    is_same_v<decltype(*std::declval<const mpfr::MpfrFloat &>()), mpfr_srcptr>);

MAGE_TEST(MpfrFloatTest, ConvertsRoundingModes) {
  using mpfr::detail::fromMpfrRoundingMode;
  using mpfr::detail::toMpfrRoundingMode;

  MAGE_EXPECT_EQ(toMpfrRoundingMode(RoundingMode::NearestTiesToEven),
                 MPFR_RNDN);
  MAGE_EXPECT_EQ(toMpfrRoundingMode(RoundingMode::Downward), MPFR_RNDD);
  MAGE_EXPECT_EQ(toMpfrRoundingMode(RoundingMode::Upward), MPFR_RNDU);
  MAGE_EXPECT_EQ(toMpfrRoundingMode(RoundingMode::TowardZero), MPFR_RNDZ);

  MAGE_EXPECT_EQ(fromMpfrRoundingMode(MPFR_RNDN),
                 RoundingMode::NearestTiesToEven);
  MAGE_EXPECT_EQ(fromMpfrRoundingMode(MPFR_RNDD), RoundingMode::Downward);
  MAGE_EXPECT_EQ(fromMpfrRoundingMode(MPFR_RNDU), RoundingMode::Upward);
  MAGE_EXPECT_EQ(fromMpfrRoundingMode(MPFR_RNDZ), RoundingMode::TowardZero);
}

MAGE_TEST(MpfrFloatTest, InitializesWithRequestedConfiguration) {
  mpfr::MpfrFloat Value(128, RoundingMode::Upward);
  const mpfr::MpfrFloat &ConstValue = Value;
  mpfr_ptr NativeValue = *Value;
  mpfr_srcptr ConstNativeValue = *ConstValue;

  MAGE_EXPECT_EQ(Value.getPrecision(), 128U);
  MAGE_EXPECT_EQ(Value.getRoundingMode(), RoundingMode::Upward);
  MAGE_EXPECT_EQ(Value.getMpfrRoundingMode(), MPFR_RNDU);
  MAGE_EXPECT_EQ(NativeValue, ConstNativeValue);
  MAGE_EXPECT_TRUE(mpfr_nan_p(NativeValue) != 0);
}

MAGE_TEST(MpfrFloatTest, SetsSupportedArithmeticTypes) {
  mpfr::MpfrFloat Value(128, RoundingMode::NearestTiesToEven);

  MAGE_EXPECT_EQ(Value.set(float16(1.5)), 0);
  MAGE_EXPECT_EQ(mpfr_cmp_d(*Value, 1.5), 0);

  MAGE_EXPECT_EQ(Value.set(-2.25f), 0);
  MAGE_EXPECT_EQ(mpfr_cmp_d(*Value, -2.25), 0);

  MAGE_EXPECT_EQ(Value.set(3.5), 0);
  MAGE_EXPECT_EQ(mpfr_cmp_d(*Value, 3.5), 0);

  MAGE_EXPECT_EQ(Value.set(-42), 0);
  MAGE_EXPECT_EQ(mpfr_cmp_si(*Value, -42), 0);

  MAGE_EXPECT_EQ(Value.set(uint64_t(123)), 0);
  MAGE_EXPECT_EQ(mpfr_cmp_ui(*Value, 123), 0);

  MAGE_EXPECT_EQ(Value.set(false), 0);
  MAGE_EXPECT_TRUE(mpfr_zero_p(*Value) != 0);

  MAGE_EXPECT_EQ(Value.set(true), 0);
  MAGE_EXPECT_EQ(mpfr_cmp_ui(*Value, 1), 0);
}

MAGE_TEST(MpfrFloatTest, RoundsInputAccordingToConfiguredMode) {
  mpfr::MpfrFloat Nearest(2, RoundingMode::NearestTiesToEven);
  mpfr::MpfrFloat Downward(2, RoundingMode::Downward);
  mpfr::MpfrFloat Upward(2, RoundingMode::Upward);
  mpfr::MpfrFloat TowardZero(2, RoundingMode::TowardZero);

  MAGE_EXPECT_TRUE(Nearest.set(1.25) < 0);
  MAGE_EXPECT_TRUE(Downward.set(-1.25) < 0);
  MAGE_EXPECT_TRUE(Upward.set(1.25) > 0);
  MAGE_EXPECT_TRUE(TowardZero.set(-1.25) > 0);

  MAGE_EXPECT_EQ(Nearest.convertTo<double>(), 1.0);
  MAGE_EXPECT_EQ(Downward.convertTo<double>(), -1.5);
  MAGE_EXPECT_EQ(Upward.convertTo<double>(), 1.5);
  MAGE_EXPECT_EQ(TowardZero.convertTo<double>(), -1.0);
}

MAGE_TEST(MpfrFloatTest, ConvertsOutputAccordingToConfiguredMode) {
  constexpr double Midpoint = 0x1.000001p0;
  constexpr float NextFloat = 0x1.000002p0f;

  mpfr::MpfrFloat Nearest(64, RoundingMode::NearestTiesToEven);
  mpfr::MpfrFloat Downward(64, RoundingMode::Downward);
  mpfr::MpfrFloat Upward(64, RoundingMode::Upward);
  mpfr::MpfrFloat TowardZero(64, RoundingMode::TowardZero);

  Nearest.set(Midpoint);
  Downward.set(Midpoint);
  Upward.set(Midpoint);
  TowardZero.set(Midpoint);

  MAGE_EXPECT_EQ(Nearest.convertTo<float>(), 1.0f);
  MAGE_EXPECT_EQ(Downward.convertTo<float>(), 1.0f);
  MAGE_EXPECT_EQ(Upward.convertTo<float>(), NextFloat);
  MAGE_EXPECT_EQ(TowardZero.convertTo<float>(), 1.0f);
}

MAGE_TEST(MpfrFloatTest, ProvidesIndependentInlineValues) {
  bool Called = false;

  mpfr::withInlineMpfrFloats<3>(
      128, RoundingMode::Downward,
      [this, &Called](mpfr::MpfrFloat &Output, mpfr::MpfrFloat &LHS,
                      mpfr::MpfrFloat &RHS) {
        Called = true;

        MAGE_EXPECT_EQ(Output.getPrecision(), 128U);
        MAGE_EXPECT_EQ(LHS.getPrecision(), 128U);
        MAGE_EXPECT_EQ(RHS.getPrecision(), 128U);
        MAGE_EXPECT_EQ(Output.getMpfrRoundingMode(), MPFR_RNDD);
        MAGE_EXPECT_TRUE(mpfr_nan_p(*Output) != 0);
        MAGE_EXPECT_NE(*Output, *LHS);
        MAGE_EXPECT_NE(*Output, *RHS);
        MAGE_EXPECT_NE(*LHS, *RHS);

        const uintptr_t OutputStorage =
            reinterpret_cast<uintptr_t>(mpfr_custom_get_significand(*Output));
        const uintptr_t LHSStorage =
            reinterpret_cast<uintptr_t>(mpfr_custom_get_significand(*LHS));
        const uintptr_t RHSStorage =
            reinterpret_cast<uintptr_t>(mpfr_custom_get_significand(*RHS));
        MAGE_EXPECT_EQ(OutputStorage % alignof(mp_limb_t), uintptr_t(0));
        MAGE_EXPECT_EQ(LHSStorage % alignof(mp_limb_t), uintptr_t(0));
        MAGE_EXPECT_EQ(RHSStorage % alignof(mp_limb_t), uintptr_t(0));
        MAGE_EXPECT_NE(OutputStorage, LHSStorage);
        MAGE_EXPECT_NE(OutputStorage, RHSStorage);
        MAGE_EXPECT_NE(LHSStorage, RHSStorage);

        LHS.set(1);
        RHS.set(2);
        MAGE_EXPECT_EQ(
            mpfr_add(*Output, *LHS, *RHS, Output.getMpfrRoundingMode()), 0);
        MAGE_EXPECT_EQ(Output.convertTo<double>(), 3.0);
      });

  MAGE_EXPECT_TRUE(Called);
}

MAGE_TEST(MpfrFloatTest, WrapsMpfrUnaryFunctions) {
  mpfr::MpfrFloat Input(128, RoundingMode::NearestTiesToEven);
  mpfr::MpfrFloat Output(128, RoundingMode::NearestTiesToEven);

  Input.set(0);
  MAGE_EXPECT_EQ(mpfr::exp(Output, Input), 0);
  MAGE_EXPECT_EQ(mpfr_cmp_ui(*Output, 1), 0);

  Input.set(1);
  MAGE_EXPECT_EQ(mpfr::log(Output, Input), 0);
  MAGE_EXPECT_TRUE(mpfr_zero_p(*Output) != 0);
}
