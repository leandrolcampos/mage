//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides MPFR-based utilities for floating-point accuracy testing.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_TESTING_MPFRFLOAT_HPP
#define MAGE_TESTING_MPFRFLOAT_HPP

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "mage/Support/FloatTypes.hpp"
#include "mage/Support/TypeTraits.hpp"

// MPFR exposes its intmax_t APIs only after <stdint.h> has been included.
#include <stdint.h>

#include <mpfr.h>

#include <assert.h>
#include <limits.h>
#include <new>
#include <stddef.h>
#include <type_traits>
#include <utility>

namespace mage {

enum class RoundingMode {
  NearestTiesToEven,
  Downward,
  Upward,
  TowardZero,
};

namespace mpfr {
namespace detail {

[[nodiscard]] RoundingMode
fromMpfrRoundingMode(mpfr_rnd_t MpfrRounding) noexcept;

[[nodiscard]] mpfr_rnd_t toMpfrRoundingMode(RoundingMode Rounding) noexcept;

} // namespace detail

template <typename T>
[[nodiscard]] constexpr unsigned getExtraPrecision() noexcept {
  using FloatType = remove_cv_t<T>;

  if constexpr (is_same_v<FloatType, float16> || is_same_v<FloatType, float>)
    return 128;
  else if constexpr (is_same_v<FloatType, double>)
    return 256;
  else
    static_assert(dependent_false_v<FloatType>,
                  "unsupported floating-point type");
}

class MpfrFloat {
public:
  explicit MpfrFloat(unsigned Precision, RoundingMode Rounding);
  ~MpfrFloat();

  MpfrFloat(const MpfrFloat &) = delete;
  MpfrFloat &operator=(const MpfrFloat &) = delete;
  MpfrFloat(MpfrFloat &&) = delete;
  MpfrFloat &operator=(MpfrFloat &&) = delete;

  template <typename T, enable_if_t<is_floating_point_v<T>, int> = 0>
  int set(T Input) noexcept {
    using FloatType = remove_cv_t<T>;

    if constexpr (is_same_v<FloatType, float16>)
      return mpfr_set_flt(Value, static_cast<float>(Input), MpfrRounding);
    else if constexpr (is_same_v<FloatType, float>)
      return mpfr_set_flt(Value, Input, MpfrRounding);
    else if constexpr (is_same_v<FloatType, double>)
      return mpfr_set_d(Value, Input, MpfrRounding);
    else
      static_assert(dependent_false_v<T>, "unsupported floating-point type");
  }

  template <typename T, enable_if_t<is_integral_v<T>, int> = 0>
  int set(T Input) noexcept {
    if constexpr (is_signed_v<T>)
      return mpfr_set_sj(Value, static_cast<intmax_t>(Input), MpfrRounding);
    else
      return mpfr_set_uj(Value, static_cast<uintmax_t>(Input), MpfrRounding);
  }

  template <typename T,
            enable_if_t<is_same_v<T, float> || is_same_v<T, double>, int> = 0>
  [[nodiscard]] T convertTo() const noexcept {
    if constexpr (is_same_v<T, float>)
      return mpfr_get_flt(Value, MpfrRounding);
    else
      return mpfr_get_d(Value, MpfrRounding);
  }

  [[nodiscard]] mpfr_ptr operator*() noexcept;
  [[nodiscard]] mpfr_srcptr operator*() const noexcept;

  [[nodiscard]] unsigned getPrecision() const noexcept;
  [[nodiscard]] RoundingMode getRoundingMode() const noexcept;
  [[nodiscard]] mpfr_rnd_t getMpfrRoundingMode() const noexcept;

private:
  template <size_t N, typename Function>
  friend void withInlineMpfrFloats(unsigned Precision, RoundingMode Rounding,
                                   Function &&Fn) noexcept;

  MpfrFloat(unsigned Precision, mpfr_rnd_t MpfrRounding,
            void *Storage) noexcept;

  mpfr_t Value;
  mpfr_rnd_t MpfrRounding;
  bool OwnsStorage;
};

namespace detail {

template <size_t> using MpfrFloatRef = MpfrFloat &;

template <typename Function, size_t... Indices>
[[nodiscard]] constexpr bool
isNothrowInvocableWithMpfrFloats(std::index_sequence<Indices...>) noexcept {
  return std::is_nothrow_invocable_v<Function, MpfrFloatRef<Indices>...>;
}

template <typename Function, size_t N, size_t... Indices>
void invokeWithMpfrFloats(Function &&Fn, MpfrFloat *const (&Values)[N],
                          std::index_sequence<Indices...>) noexcept {
  std::forward<Function>(Fn)(*Values[Indices]...);
}

} // namespace detail

/// Invokes \p Fn with references to N MPFR values using stack storage.
///
/// The references and pointers derived from them must not escape the
/// invocation.
template <size_t N, typename Function>
void withInlineMpfrFloats(unsigned Precision, RoundingMode Rounding,
                          Function &&Fn) noexcept {
  static_assert(N > 0, "N must be greater than zero");
  static_assert(detail::isNothrowInvocableWithMpfrFloats<Function &&>(
                    std::make_index_sequence<N>{}),
                "Fn must be noexcept");

  if constexpr (N > 0) {
    assert((Precision >= MPFR_PREC_MIN) &&
           "Precision must not be less than MPFR_PREC_MIN");
    assert((Precision <= static_cast<unsigned long>(MPFR_PREC_MAX)) &&
           "Precision must not exceed MPFR_PREC_MAX");

    const size_t SignificandSize =
        mpfr_custom_get_size(static_cast<mpfr_prec_t>(Precision));

    assert((SignificandSize <= SIZE_MAX / N) &&
           "inline MPFR storage size must be representable");

    const size_t StorageSize = N * SignificandSize;
    auto *Storage = static_cast<unsigned char *>(__builtin_alloca_with_align(
        StorageSize, alignof(mp_limb_t) * CHAR_BIT));

    alignas(MpfrFloat) unsigned char ObjectStorage[N][sizeof(MpfrFloat)];
    MpfrFloat *Values[N];
    const mpfr_rnd_t MpfrRounding = detail::toMpfrRoundingMode(Rounding);

    for (size_t I = 0; I < N; ++I) {
      void *ObjectAddress = static_cast<void *>(ObjectStorage[I]);
      void *ValueStorage = static_cast<void *>(Storage + I * SignificandSize);
      Values[I] = ::new (ObjectAddress)
          MpfrFloat(Precision, MpfrRounding, ValueStorage);
    }

    detail::invokeWithMpfrFloats(std::forward<Function>(Fn), Values,
                                 std::make_index_sequence<N>{});

    // Objects created with placement new must be destroyed explicitly.
    for (size_t I = N; I > 0; --I)
      Values[I - 1]->~MpfrFloat();
  }
}

int exp(MpfrFloat &Output, const MpfrFloat &Input) noexcept;
int log(MpfrFloat &Output, const MpfrFloat &Input) noexcept;

} // namespace mpfr
} // namespace mage

#endif // MAGE_TESTING_MPFRFLOAT_HPP
