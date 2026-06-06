//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides bit manipulation utilities.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_MATHEXTRAS_BIT_HPP
#define MAGE_MATHEXTRAS_BIT_HPP

#include "mage/MathExtras/TypeTraits.hpp"

namespace mage {
namespace numeric {

template <typename To, typename From>
[[nodiscard]] constexpr To
bit_cast(const From &Value) noexcept // NOLINT(readability-identifier-naming)
{
  static_assert(sizeof(To) == sizeof(From), "bit_cast requires equal sizes");
  static_assert(is_trivially_copyable_v<To>, "To must be trivially copyable");
  static_assert(is_trivially_copyable_v<From>,
                "From must be trivially copyable");

  return __builtin_bit_cast(To, Value);
}

} // namespace numeric
} // namespace mage

#endif // MAGE_MATHEXTRAS_BIT_HPP
