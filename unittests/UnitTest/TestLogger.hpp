//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares logging support for Mage unit tests.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_UNITTESTS_UNITTEST_TESTLOGGER_HPP
#define MAGE_UNITTESTS_UNITTEST_TESTLOGGER_HPP

#include "mage/Support/TypeTraits.hpp"

namespace mage {
namespace testing {

// A class to log to standard error in the context of unit tests.
struct TestLogger {
  constexpr TestLogger() noexcept = default;

  TestLogger &operator<<(const char *) noexcept;
  TestLogger &operator<<(decltype(nullptr)) noexcept;
  TestLogger &operator<<(char) noexcept;
  TestLogger &operator<<(bool) noexcept;

  TestLogger &operator<<(short) noexcept;
  TestLogger &operator<<(unsigned short) noexcept;
  TestLogger &operator<<(int) noexcept;
  TestLogger &operator<<(unsigned) noexcept;
  TestLogger &operator<<(long) noexcept;
  TestLogger &operator<<(unsigned long) noexcept;
  TestLogger &operator<<(long long) noexcept;
  TestLogger &operator<<(unsigned long long) noexcept;

  TestLogger &operator<<(const void *) noexcept;

  TestLogger &operator<<(_Float16) noexcept;
  TestLogger &operator<<(float) noexcept;
  TestLogger &operator<<(double) noexcept;

  template <typename Enum, enable_if_t<is_enum_v<Enum>, int> = 0>
  TestLogger &operator<<(Enum Value) noexcept {
    using UnderlyingTy = underlying_type_t<Enum>;

    if constexpr (is_signed_v<UnderlyingTy>)
      return *this << static_cast<long long>(Value);
    else
      return *this << static_cast<unsigned long long>(Value);
  }
};

// Returns the global test logger to be used in unit tests.
[[nodiscard]] TestLogger &tlog() noexcept;

} // namespace testing
} // namespace mage

#endif // MAGE_UNITTESTS_UNITTEST_TESTLOGGER_HPP
