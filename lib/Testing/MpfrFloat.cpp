//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements MPFR-based utilities for floating-point accuracy testing.
///
//===----------------------------------------------------------------------===//

#include "mage/Testing/MpfrFloat.hpp"

#include <mpfr.h>

#include <assert.h>

using namespace mage;

RoundingMode
mpfr::detail::fromMpfrRoundingMode(mpfr_rnd_t MpfrRounding) noexcept {
  switch (MpfrRounding) {
  case MPFR_RNDN:
    return RoundingMode::NearestTiesToEven;
  case MPFR_RNDD:
    return RoundingMode::Downward;
  case MPFR_RNDU:
    return RoundingMode::Upward;
  case MPFR_RNDZ:
    return RoundingMode::TowardZero;
  default:
    // TODO: Use MAGE_UNREACHABLE once available.
    __builtin_unreachable();
  }
}

mpfr_rnd_t mpfr::detail::toMpfrRoundingMode(RoundingMode Rounding) noexcept {
  switch (Rounding) {
  case RoundingMode::NearestTiesToEven:
    return MPFR_RNDN;
  case RoundingMode::Downward:
    return MPFR_RNDD;
  case RoundingMode::Upward:
    return MPFR_RNDU;
  case RoundingMode::TowardZero:
    return MPFR_RNDZ;
  }

  __builtin_unreachable();
}

namespace mage {
namespace mpfr {

MpfrFloat::MpfrFloat(unsigned Precision, RoundingMode Rounding)
    : MpfrRounding(detail::toMpfrRoundingMode(Rounding)), OwnsStorage(true) {
  const mpfr_prec_t MpfrPrecision = static_cast<mpfr_prec_t>(Precision);

  assert((MpfrPrecision >= MPFR_PREC_MIN) &&
         "Precision must not be less than MPFR_PREC_MIN");
  assert((MpfrPrecision <= MPFR_PREC_MAX) &&
         "Precision must not exceed MPFR_PREC_MAX");
  assert((static_cast<unsigned long>(MpfrPrecision) == Precision) &&
         "Precision must be representable by mpfr_prec_t");

  mpfr_init2(Value, MpfrPrecision);
}

MpfrFloat::MpfrFloat(unsigned Precision, mpfr_rnd_t MpfrRounding,
                     void *Storage) noexcept
    : MpfrRounding(MpfrRounding), OwnsStorage(false) {
  assert(Storage && "Storage must not be null");

  const mpfr_prec_t MpfrPrecision = static_cast<mpfr_prec_t>(Precision);
  mpfr_custom_init(Storage, MpfrPrecision);
  mpfr_custom_init_set(Value, MPFR_NAN_KIND, 0, MpfrPrecision, Storage);
}

MpfrFloat::~MpfrFloat() noexcept {
  if (OwnsStorage)
    mpfr_clear(Value);
}

mpfr_ptr MpfrFloat::operator*() noexcept { return Value; }

mpfr_srcptr MpfrFloat::operator*() const noexcept { return Value; }

unsigned MpfrFloat::getPrecision() const noexcept {
  return static_cast<unsigned>(mpfr_get_prec(Value));
}

RoundingMode MpfrFloat::getRoundingMode() const noexcept {
  return detail::fromMpfrRoundingMode(MpfrRounding);
}

mpfr_rnd_t MpfrFloat::getMpfrRoundingMode() const noexcept {
  return MpfrRounding;
}

} // namespace mpfr
} // namespace mage

int mpfr::exp(MpfrFloat &Output, const MpfrFloat &Input) noexcept {
  return mpfr_exp(*Output, *Input, Output.getMpfrRoundingMode());
}

int mpfr::log(MpfrFloat &Output, const MpfrFloat &Input) noexcept {
  return mpfr_log(*Output, *Input, Output.getMpfrRoundingMode());
}
