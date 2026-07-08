//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests type traits for numeric code.
///
//===----------------------------------------------------------------------===//

#include "mage/Support/TypeTraits.hpp"
#include "UnitTest/Test.hpp"

#include <stdint.h>

using namespace mage;

struct UnsupportedType;

namespace {

enum UnscopedEnum { UnscopedValue };
enum class SignedEnum : signed char { Value = -1 };
enum class UnsignedEnum : unsigned long long { Value = 1 };

} // namespace

template <typename T, enable_if_t<is_same_v<T, int>, int> = 0>
static constexpr bool isSelectedByEnableIf(T) {
  return true;
}

template <typename T, enable_if_t<!is_same_v<T, int>, int> = 0>
static constexpr bool isSelectedByEnableIf(T) {
  return false;
}

//===----------------------------------------------------------------------===//
// Basic type utilities
//===----------------------------------------------------------------------===//

static_assert(true_type::value, "true_type stores true");
static_assert(!false_type::value, "false_type stores false");

static_assert(!dependent_false_v<int>, "dependent_false_v is false for int");
static_assert(!dependent_false_v<UnsupportedType>,
              "dependent_false_v is false for unsupported types");

static_assert(__is_same(typename enable_if<true, int>::type, int),
              "enable_if exposes its selected type when true");
static_assert(__is_same(enable_if_t<true, unsigned int>, unsigned int),
              "enable_if_t aliases its selected type when true");
static_assert(__is_same(enable_if_t<true>, void),
              "enable_if_t defaults its selected type to void");
static_assert(isSelectedByEnableIf(0),
              "enable_if preserves an overload when true");
static_assert(!isSelectedByEnableIf(0.0),
              "enable_if removes an overload when false");

static_assert(__is_same(type_identity_t<int>, int),
              "type_identity_t preserves int");
static_assert(__is_same(type_identity_t<const volatile int>,
                        const volatile int),
              "type_identity_t preserves const volatile int");

static_assert(is_same_v<int, int>, "is_same_v detects identical types");
static_assert(!is_same_v<int, unsigned int>,
              "is_same_v rejects different types");
static_assert(!is_same_v<const int, int>,
              "is_same_v preserves const qualification");
static_assert(!is_same_v<volatile int, int>,
              "is_same_v preserves volatile qualification");
static_assert(is_same_v<const volatile int, const volatile int>,
              "is_same_v detects identical cv-qualified types");

static_assert(type_list_size_v<type_list<>> == 0,
              "empty type_list has no elements");
static_assert(type_list_size_v<type_list<int, float, double>> == 3,
              "type_list_size_v counts elements");
static_assert(__is_same(type_list_element_t<0, type_list<int, float, double>>,
                        int),
              "type_list_element_t selects the first element");
static_assert(__is_same(type_list_element_t<1, type_list<int, float, double>>,
                        float),
              "type_list_element_t selects the second element");
static_assert(__is_same(type_list_element_t<2, type_list<int, float, double>>,
                        double),
              "type_list_element_t selects the third element");

//===----------------------------------------------------------------------===//
// cv-qualifier transformations
//===----------------------------------------------------------------------===//

static_assert(__is_same(typename remove_const<int>::type, int),
              "remove_const preserves unqualified int");
static_assert(__is_same(typename remove_const<volatile int>::type,
                        volatile int),
              "remove_const preserves volatile");
static_assert(__is_same(typename remove_const<const int>::type, int),
              "remove_const removes const");
static_assert(__is_same(typename remove_const<const volatile int>::type,
                        volatile int),
              "remove_const removes const from const volatile");

static_assert(__is_same(typename remove_volatile<int>::type, int),
              "remove_volatile preserves unqualified int");
static_assert(__is_same(typename remove_volatile<const int>::type, const int),
              "remove_volatile preserves const");
static_assert(__is_same(typename remove_volatile<volatile int>::type, int),
              "remove_volatile removes volatile");
static_assert(__is_same(typename remove_volatile<const volatile int>::type,
                        const int),
              "remove_volatile removes volatile from const volatile");

static_assert(__is_same(remove_cv_t<int>, int),
              "remove_cv_t preserves unqualified int");
static_assert(__is_same(remove_cv_t<const int>, int),
              "remove_cv_t removes const");
static_assert(__is_same(remove_cv_t<volatile int>, int),
              "remove_cv_t removes volatile");
static_assert(__is_same(remove_cv_t<const volatile int>, int),
              "remove_cv_t removes const volatile");

static_assert(__is_same(copy_cv_t<int, unsigned int>, unsigned int),
              "copy_cv_t copies no qualifiers from unqualified source");
static_assert(__is_same(copy_cv_t<const int, unsigned int>, const unsigned int),
              "copy_cv_t copies const");
static_assert(__is_same(copy_cv_t<volatile int, unsigned int>,
                        volatile unsigned int),
              "copy_cv_t copies volatile");
static_assert(__is_same(copy_cv_t<const volatile int, unsigned int>,
                        const volatile unsigned int),
              "copy_cv_t copies const volatile");

//===----------------------------------------------------------------------===//
// Type classification
//===----------------------------------------------------------------------===//

static_assert(is_integral_v<bool>, "bool is integral");
static_assert(is_integral_v<char>, "char is integral");
static_assert(is_integral_v<signed char>, "signed char is integral");
static_assert(is_integral_v<unsigned char>, "unsigned char is integral");
static_assert(is_integral_v<short>, "short is integral");
static_assert(is_integral_v<unsigned short>, "unsigned short is integral");
static_assert(is_integral_v<int>, "int is integral");
static_assert(is_integral_v<unsigned int>, "unsigned int is integral");
static_assert(is_integral_v<long>, "long is integral");
static_assert(is_integral_v<unsigned long>, "unsigned long is integral");
static_assert(is_integral_v<long long>, "long long is integral");
static_assert(is_integral_v<unsigned long long>,
              "unsigned long long is integral");
static_assert(is_integral_v<const volatile int>,
              "cv-qualified int is integral");

static_assert(!is_integral_v<_Float16>, "_Float16 is not integral");
static_assert(!is_integral_v<float>, "float is not integral");
static_assert(!is_integral_v<double>, "double is not integral");
static_assert(!is_integral_v<long double>, "long double is not integral");

static_assert(!is_integral_v<UnsupportedType>,
              "unsupported type is not integral");

static_assert(is_floating_point_v<_Float16>, "_Float16 is floating point");
static_assert(is_floating_point_v<float>, "float is floating point");
static_assert(is_floating_point_v<double>, "double is floating point");
static_assert(is_floating_point_v<const volatile double>,
              "cv-qualified double is floating point");

static_assert(!is_floating_point_v<long double>,
              "long double is outside Mage floating-point support");

static_assert(!is_floating_point_v<bool>, "bool is not floating point");
static_assert(!is_floating_point_v<char>, "char is not floating point");
static_assert(!is_floating_point_v<signed char>,
              "signed char is not floating point");
static_assert(!is_floating_point_v<unsigned char>,
              "unsigned char is not floating point");
static_assert(!is_floating_point_v<short>, "short is not floating point");
static_assert(!is_floating_point_v<unsigned short>,
              "unsigned short is not floating point");
static_assert(!is_floating_point_v<int>, "int is not floating point");
static_assert(!is_floating_point_v<unsigned int>,
              "unsigned int is not floating point");
static_assert(!is_floating_point_v<long>, "long is not floating point");
static_assert(!is_floating_point_v<unsigned long>,
              "unsigned long is not floating point");
static_assert(!is_floating_point_v<long long>,
              "long long is not floating point");
static_assert(!is_floating_point_v<unsigned long long>,
              "unsigned long long is not floating point");

static_assert(!is_floating_point_v<UnsupportedType>,
              "unsupported type is not floating point");

static_assert(is_arithmetic_v<bool>, "bool is arithmetic");
static_assert(is_arithmetic_v<int>, "int is arithmetic");
static_assert(is_arithmetic_v<unsigned long long>,
              "unsigned long long is arithmetic");
static_assert(is_arithmetic_v<_Float16>, "_Float16 is arithmetic");
static_assert(is_arithmetic_v<float>, "float is arithmetic");
static_assert(is_arithmetic_v<double>, "double is arithmetic");
static_assert(is_arithmetic_v<const volatile int>,
              "cv-qualified int is arithmetic");
static_assert(is_arithmetic_v<const volatile double>,
              "cv-qualified double is arithmetic");

static_assert(!is_arithmetic_v<long double>,
              "long double is outside Mage arithmetic support");
static_assert(!is_arithmetic_v<UnsupportedType>,
              "unsupported type is not arithmetic");

static_assert(is_arithmetic<int>::value == is_arithmetic_v<int>);

static_assert(is_enum_v<UnscopedEnum>, "unscoped enum is an enum");
static_assert(is_enum_v<SignedEnum>, "scoped signed enum is an enum");
static_assert(is_enum_v<UnsignedEnum>, "scoped unsigned enum is an enum");
static_assert(is_enum_v<const volatile SignedEnum>,
              "cv-qualified enum is an enum");

static_assert(!is_enum_v<int>, "int is not an enum");
static_assert(!is_enum_v<UnsupportedType>, "unsupported type is not an enum");

static_assert(is_enum<SignedEnum>::value == is_enum_v<SignedEnum>);

static_assert(is_signed_v<signed char>, "signed char is signed");
static_assert(is_signed_v<short>, "short is signed");
static_assert(is_signed_v<int>, "int is signed");
static_assert(is_signed_v<long>, "long is signed");
static_assert(is_signed_v<long long>, "long long is signed");
static_assert(is_signed_v<_Float16>, "_Float16 is signed");
static_assert(is_signed_v<float>, "float is signed");
static_assert(is_signed_v<double>, "double is signed");

static_assert(!is_signed_v<bool>, "bool is not signed");
static_assert(!is_signed_v<unsigned char>, "unsigned char is not signed");
static_assert(!is_signed_v<unsigned short>, "unsigned short is not signed");
static_assert(!is_signed_v<unsigned int>, "unsigned int is not signed");
static_assert(!is_signed_v<unsigned long>, "unsigned long is not signed");
static_assert(!is_signed_v<unsigned long long>,
              "unsigned long long is not signed");
static_assert(!is_signed_v<long double>,
              "long double is outside Mage signedness support");

static_assert(is_signed_v<const volatile int>,
              "cv-qualified signed int is signed");

static_assert(!is_signed_v<UnsupportedType>, "unsupported type is not signed");

static_assert(is_unsigned_v<bool>, "bool is unsigned");
static_assert(is_unsigned_v<unsigned char>, "unsigned char is unsigned");
static_assert(is_unsigned_v<unsigned short>, "unsigned short is unsigned");
static_assert(is_unsigned_v<unsigned int>, "unsigned int is unsigned");
static_assert(is_unsigned_v<unsigned long>, "unsigned long is unsigned");
static_assert(is_unsigned_v<unsigned long long>,
              "unsigned long long is unsigned");

static_assert(!is_unsigned_v<signed char>, "signed char is not unsigned");
static_assert(!is_unsigned_v<short>, "short is not unsigned");
static_assert(!is_unsigned_v<int>, "int is not unsigned");
static_assert(!is_unsigned_v<long>, "long is not unsigned");
static_assert(!is_unsigned_v<long long>, "long long is not unsigned");
static_assert(!is_unsigned_v<_Float16>, "_Float16 is not unsigned");
static_assert(!is_unsigned_v<float>, "float is not unsigned");
static_assert(!is_unsigned_v<double>, "double is not unsigned");
static_assert(!is_unsigned_v<long double>,
              "long double is outside Mage unsignedness support");

static_assert(is_unsigned_v<const unsigned int>,
              "cv-qualified unsigned int is unsigned");

static_assert(!is_unsigned_v<UnsupportedType>,
              "unsupported type is not unsigned");

//===----------------------------------------------------------------------===//
// Type properties
//===----------------------------------------------------------------------===//

namespace {

struct TriviallyCopyable {
  int Value;
};

struct NonTriviallyCopyable {
  NonTriviallyCopyable(const NonTriviallyCopyable &) {}

  int Value;
};

} // namespace

static_assert(is_trivially_copyable_v<int>);
static_assert(is_trivially_copyable_v<const int>);
static_assert(is_trivially_copyable_v<TriviallyCopyable>);
static_assert(!is_trivially_copyable_v<NonTriviallyCopyable>);

static_assert(is_trivially_copyable<int>::value ==
              is_trivially_copyable_v<int>);

//===----------------------------------------------------------------------===//
// Type transformations
//===----------------------------------------------------------------------===//

static_assert(__is_same(underlying_type_t<UnscopedEnum>,
                        __underlying_type(UnscopedEnum)),
              "underlying_type_t exposes an unscoped enum's underlying type");
static_assert(__is_same(underlying_type_t<SignedEnum>, signed char),
              "underlying_type_t preserves signed enum underlying type");
static_assert(__is_same(underlying_type_t<UnsignedEnum>, unsigned long long),
              "underlying_type_t preserves unsigned enum underlying type");

static_assert(__is_same(make_unsigned_t<char>, unsigned char),
              "make_unsigned_t maps char to unsigned char");
static_assert(__is_same(make_unsigned_t<signed char>, unsigned char),
              "make_unsigned_t maps signed char to unsigned char");
static_assert(__is_same(make_unsigned_t<unsigned char>, unsigned char),
              "make_unsigned_t preserves unsigned char");
static_assert(__is_same(make_unsigned_t<short>, unsigned short),
              "make_unsigned_t maps short to unsigned short");
static_assert(__is_same(make_unsigned_t<unsigned short>, unsigned short),
              "make_unsigned_t preserves unsigned short");
static_assert(__is_same(make_unsigned_t<int>, unsigned int),
              "make_unsigned_t maps int to unsigned int");
static_assert(__is_same(make_unsigned_t<unsigned int>, unsigned int),
              "make_unsigned_t preserves unsigned int");
static_assert(__is_same(make_unsigned_t<long>, unsigned long),
              "make_unsigned_t maps long to unsigned long");
static_assert(__is_same(make_unsigned_t<unsigned long>, unsigned long),
              "make_unsigned_t preserves unsigned long");
static_assert(__is_same(make_unsigned_t<long long>, unsigned long long),
              "make_unsigned_t maps long long to unsigned long long");
static_assert(__is_same(make_unsigned_t<unsigned long long>,
                        unsigned long long),
              "make_unsigned_t preserves unsigned long long");

static_assert(__is_same(make_unsigned_t<const int>, const unsigned int),
              "make_unsigned_t preserves const");
static_assert(__is_same(make_unsigned_t<volatile int>, volatile unsigned int),
              "make_unsigned_t preserves volatile");
static_assert(__is_same(make_unsigned_t<const volatile int>,
                        const volatile unsigned int),
              "make_unsigned_t preserves const volatile");

//===----------------------------------------------------------------------===//
// Function type traits
//===----------------------------------------------------------------------===//

using TestFunction = int(float, double *);
using NoexceptTestFunction = void(const int *) noexcept;

static_assert(function_traits<TestFunction>::parameter_count == 2,
              "function_traits counts parameters of a function");
static_assert(__is_same(function_return_type_t<TestFunction>, int),
              "function_return_type_t exposes the return type of a function");
static_assert(
    __is_same(function_parameter_types_t<TestFunction>,
              type_list<float, double *>),
    "function_parameter_types_t exposes parameter types of a function");

static_assert(function_traits<NoexceptTestFunction>::parameter_count == 1,
              "function_traits counts parameters of a noexcept function");
static_assert(
    __is_same(function_return_type_t<NoexceptTestFunction>, void),
    "function_return_type_t exposes the return type of a noexcept function");
static_assert(__is_same(function_parameter_types_t<NoexceptTestFunction>,
                        type_list<const int *>),
              "function_parameter_types_t exposes parameter types of a "
              "noexcept function");

static_assert(function_traits<TestFunction *>::parameter_count == 2,
              "function_traits counts parameters of a function pointer");
static_assert(
    __is_same(function_return_type_t<TestFunction *>, int),
    "function_return_type_t exposes the return type of a function pointer");
static_assert(
    __is_same(function_parameter_types_t<TestFunction *>,
              type_list<float, double *>),
    "function_parameter_types_t exposes parameter types of a function pointer");

static_assert(
    function_traits<NoexceptTestFunction *>::parameter_count == 1,
    "function_traits counts parameters of a pointer to noexcept function");
static_assert(__is_same(function_return_type_t<NoexceptTestFunction *>, void),
              "function_return_type_t exposes the return type of a pointer to "
              "noexcept function");
static_assert(__is_same(function_parameter_types_t<NoexceptTestFunction *>,
                        type_list<const int *>),
              "function_parameter_types_t exposes parameter types of a pointer "
              "to noexcept function");

//===----------------------------------------------------------------------===//
// Numeric storage types
//===----------------------------------------------------------------------===//

static_assert(__is_same(storage_type_t<bool>, bool),
              "storage_type_t preserves bool");
static_assert(__is_same(storage_type_t<signed char>, unsigned char),
              "storage_type_t maps signed char to unsigned char");
static_assert(__is_same(storage_type_t<unsigned char>, unsigned char),
              "storage_type_t preserves unsigned char");
static_assert(__is_same(storage_type_t<short>, unsigned short),
              "storage_type_t maps short to unsigned short");
static_assert(__is_same(storage_type_t<unsigned short>, unsigned short),
              "storage_type_t preserves unsigned short");
static_assert(__is_same(storage_type_t<int>, unsigned int),
              "storage_type_t maps int to unsigned int");
static_assert(__is_same(storage_type_t<unsigned int>, unsigned int),
              "storage_type_t preserves unsigned int");
static_assert(__is_same(storage_type_t<long>, unsigned long),
              "storage_type_t maps long to unsigned long");
static_assert(__is_same(storage_type_t<unsigned long>, unsigned long),
              "storage_type_t preserves unsigned long");
static_assert(__is_same(storage_type_t<long long>, unsigned long long),
              "storage_type_t maps long long to unsigned long long");
static_assert(__is_same(storage_type_t<unsigned long long>, unsigned long long),
              "storage_type_t preserves unsigned long long");

static_assert(is_unsigned_v<char>
                  ? __is_same(storage_type_t<char>, char)
                  : __is_same(storage_type_t<char>, unsigned char),
              "storage_type_t follows char signedness");
static_assert(__is_same(storage_type_t<const int>, unsigned int),
              "storage_type_t removes const");
static_assert(__is_same(storage_type_t<volatile int>, unsigned int),
              "storage_type_t removes volatile");
static_assert(__is_same(storage_type_t<const volatile int>, unsigned int),
              "storage_type_t removes const volatile");

static_assert(__is_same(storage_type_t<_Float16>, uint16_t),
              "storage_type_t maps _Float16 to uint16_t");
static_assert(__is_same(storage_type_t<float>, uint32_t),
              "storage_type_t maps float to uint32_t");
static_assert(__is_same(storage_type_t<double>, uint64_t),
              "storage_type_t maps double to uint64_t");
static_assert(__is_same(storage_type_t<const volatile double>, uint64_t),
              "storage_type_t removes const volatile from double");

MAGE_TEST(TypeTraitsTest, CompileTimeChecks) {}
