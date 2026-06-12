//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides type traits for numeric code.
///
/// These traits intentionally avoid depending on the C++ standard library so
/// they can be used by code that may also be compiled for device targets.
///
/// These traits model only the scalar types supported by Mage numeric code.
///
//===----------------------------------------------------------------------===//

// NOLINTBEGIN(readability-identifier-naming)

#ifndef MAGE_MATHEXTRAS_TYPETRAITS_HPP
#define MAGE_MATHEXTRAS_TYPETRAITS_HPP

#include "mage/MathExtras/FloatTypes.hpp"

#include <stdint.h>

namespace mage {

//===----------------------------------------------------------------------===//
// Basic type utilities
//===----------------------------------------------------------------------===//

namespace detail {

template <typename T, T V> struct integral_constant {
  static constexpr T value = V;

  using value_type = T;
  using type = integral_constant<T, V>;

  constexpr operator value_type() const noexcept { return value; }
  constexpr value_type operator()() const noexcept { return value; }
};

template <bool B> using bool_constant = integral_constant<bool, B>;

} // namespace detail

using true_type = detail::bool_constant<true>;
using false_type = detail::bool_constant<false>;

template <typename> inline constexpr bool dependent_false_v = false;

template <bool B, typename T = void> struct enable_if {};

template <typename T> struct enable_if<true, T> {
  using type = T;
};

template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

template <typename T> struct type_identity {
  using type = T;
};

template <typename T> using type_identity_t = typename type_identity<T>::type;

template <typename T, typename U> struct is_same : false_type {};

template <typename T> struct is_same<T, T> : true_type {};

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

//===----------------------------------------------------------------------===//
// cv-qualifier transformations
//===----------------------------------------------------------------------===//

template <typename T> struct remove_const {
  using type = T;
};

template <typename T> struct remove_const<const T> {
  using type = T;
};

template <typename T> struct remove_volatile {
  using type = T;
};

template <typename T> struct remove_volatile<volatile T> {
  using type = T;
};

template <typename T> struct remove_cv {
  using type = typename remove_const<typename remove_volatile<T>::type>::type;
};

template <typename T> using remove_cv_t = typename remove_cv<T>::type;

template <typename From, typename To> struct copy_cv {
  using type = To;
};

template <typename From, typename To> struct copy_cv<const From, To> {
  using type = const To;
};

template <typename From, typename To> struct copy_cv<volatile From, To> {
  using type = volatile To;
};

template <typename From, typename To> struct copy_cv<const volatile From, To> {
  using type = const volatile To;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

//===----------------------------------------------------------------------===//
// Type classification
//===----------------------------------------------------------------------===//

namespace detail {

template <typename T> struct is_integral_impl : false_type {};

template <> struct is_integral_impl<bool> : true_type {};

template <> struct is_integral_impl<char> : true_type {};
template <> struct is_integral_impl<signed char> : true_type {};
template <> struct is_integral_impl<unsigned char> : true_type {};

template <> struct is_integral_impl<short> : true_type {};
template <> struct is_integral_impl<unsigned short> : true_type {};

template <> struct is_integral_impl<int> : true_type {};
template <> struct is_integral_impl<unsigned int> : true_type {};

template <> struct is_integral_impl<long> : true_type {};
template <> struct is_integral_impl<unsigned long> : true_type {};

template <> struct is_integral_impl<long long> : true_type {};
template <> struct is_integral_impl<unsigned long long> : true_type {};

} // namespace detail

template <typename T>
struct is_integral : detail::is_integral_impl<remove_cv_t<T>> {};

template <typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

namespace detail {

template <typename T> struct is_floating_point_impl : false_type {};

template <> struct is_floating_point_impl<float16> : true_type {};
template <> struct is_floating_point_impl<float> : true_type {};
template <> struct is_floating_point_impl<double> : true_type {};

} // namespace detail

template <typename T>
struct is_floating_point : detail::is_floating_point_impl<remove_cv_t<T>> {};

template <typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

template <typename T>
struct is_arithmetic
    : detail::bool_constant<is_integral_v<T> || is_floating_point_v<T>> {};

template <typename T>
inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

template <typename T> struct is_enum : detail::bool_constant<__is_enum(T)> {};

template <typename T> inline constexpr bool is_enum_v = is_enum<T>::value;

namespace detail {

template <typename T, bool IsArithmetic = is_arithmetic_v<T>>
struct is_signed_impl : false_type {};

template <typename T>
struct is_signed_impl<T, true>
    : bool_constant<(static_cast<T>(-1) < static_cast<T>(0))> {};

template <typename T, bool IsArithmetic = is_arithmetic_v<T>>
struct is_unsigned_impl : false_type {};

template <typename T>
struct is_unsigned_impl<T, true>
    : bool_constant<(static_cast<T>(0) < static_cast<T>(-1))> {};

} // namespace detail

template <typename T>
struct is_signed : detail::is_signed_impl<remove_cv_t<T>> {};

template <typename T> inline constexpr bool is_signed_v = is_signed<T>::value;

template <typename T>
struct is_unsigned : detail::is_unsigned_impl<remove_cv_t<T>> {};

template <typename T>
inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

//===----------------------------------------------------------------------===//
// Type properties
//===----------------------------------------------------------------------===//

template <typename T>
struct is_trivially_copyable
    : detail::bool_constant<__is_trivially_copyable(T)> {};

template <typename T>
inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<T>::value;

//===----------------------------------------------------------------------===//
// Type transformations
//===----------------------------------------------------------------------===//

template <typename T, bool IsEnum = is_enum_v<T>> struct underlying_type {};

template <typename T> struct underlying_type<T, true> {
  using type = __underlying_type(T);
};

template <typename T>
using underlying_type_t = typename underlying_type<T>::type;

namespace detail {

template <typename T> struct make_unsigned_impl;

template <> struct make_unsigned_impl<char> {
  using type = unsigned char;
};

template <> struct make_unsigned_impl<signed char> {
  using type = unsigned char;
};

template <> struct make_unsigned_impl<unsigned char> {
  using type = unsigned char;
};

template <> struct make_unsigned_impl<short> {
  using type = unsigned short;
};

template <> struct make_unsigned_impl<unsigned short> {
  using type = unsigned short;
};

template <> struct make_unsigned_impl<int> {
  using type = unsigned int;
};

template <> struct make_unsigned_impl<unsigned int> {
  using type = unsigned int;
};

template <> struct make_unsigned_impl<long> {
  using type = unsigned long;
};

template <> struct make_unsigned_impl<unsigned long> {
  using type = unsigned long;
};

template <> struct make_unsigned_impl<long long> {
  using type = unsigned long long;
};

template <> struct make_unsigned_impl<unsigned long long> {
  using type = unsigned long long;
};

} // namespace detail

template <typename T> struct make_unsigned {
private:
  using unsigned_type =
      typename detail::make_unsigned_impl<remove_cv_t<T>>::type;

public:
  using type = copy_cv_t<T, unsigned_type>;
};

template <typename T> using make_unsigned_t = typename make_unsigned<T>::type;

//===----------------------------------------------------------------------===//
// Numeric storage types
//===----------------------------------------------------------------------===//

namespace detail {

template <typename T> struct fp_storage_type;

template <> struct fp_storage_type<float16> {
  using type = uint16_t;
};

template <> struct fp_storage_type<float> {
  using type = uint32_t;
};

template <> struct fp_storage_type<double> {
  using type = uint64_t;
};

template <typename T>
using fp_storage_type_t = typename fp_storage_type<T>::type;

} // namespace detail

template <typename T> class storage_type {
private:
  static constexpr auto getStorageType() noexcept {
    if constexpr (is_floating_point_v<T>)
      return type_identity<detail::fp_storage_type_t<remove_cv_t<T>>>{};
    else if constexpr (is_unsigned_v<T>)
      return type_identity<remove_cv_t<T>>{};
    else if constexpr (is_signed_v<T>)
      return type_identity<make_unsigned_t<remove_cv_t<T>>>{};
    else
      static_assert(dependent_false_v<T>, "unsupported type");
  }

public:
  using type = typename decltype(getStorageType())::type;
};

template <typename T> using storage_type_t = typename storage_type<T>::type;

} // namespace mage

#endif // MAGE_MATHEXTRAS_TYPETRAITS_HPP

// NOLINTEND(readability-identifier-naming)
