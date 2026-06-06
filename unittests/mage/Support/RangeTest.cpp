//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Tests numeric ranges.
///
//===----------------------------------------------------------------------===//

#include "mage/Support/Range.hpp"
#include "UnitTest/Test.hpp"
#include "UnitTest/TestLogger.hpp"

#include "mage/MathExtras/Bit.hpp"
#include "mage/MathExtras/FloatTypes.hpp"
#include "mage/MathExtras/TypeTraits.hpp"

#include <float.h>
#include <stddef.h>
#include <stdint.h>

using namespace mage::numeric;

#define EXPECT_VALUE_EQ(LHS, RHS)                                              \
  (void)this->expectValueEQ((LHS), (RHS), #LHS, #RHS, MAGE_TEST_LOC_())

#define EXPECT_RANGE_EQ(Inclusive, Begin, End, Stride, Expected)               \
  this->template expectRangeEQ<Inclusive>((Begin), (End), (Stride),            \
                                          (Expected), MAGE_TEST_LOC_())

template <typename T> static constexpr auto getValueForComparison(T Value) {
  if constexpr (is_floating_point_v<T>) {
    // TODO: Use FPBits to access the storage representation once available.
    return bit_cast<storage_type_t<T>>(Value);
  } else {
    return Value;
  }
}

template <typename T> static constexpr uint64_t getFiniteSymmetricSize(T Max) {
  using StorageType = storage_type_t<T>;
  const StorageType Magnitude = bit_cast<StorageType>(Max);
  return static_cast<uint64_t>(Magnitude) * 2 + 2;
}

template <typename T> static constexpr T getPreviousPositiveValue(T Value) {
  using StorageType = storage_type_t<T>;
  const StorageType Bits = bit_cast<StorageType>(Value);
  return bit_cast<T>(static_cast<StorageType>(Bits - 1));
}

template <typename T> static const char *getTypeName() {
  if constexpr (is_same_v<T, mage::float16>)
    return "float16";
  else if constexpr (is_same_v<T, float>)
    return "float";
  else if constexpr (is_same_v<T, double>)
    return "double";
  else if constexpr (is_same_v<T, int16_t>)
    return "int16_t";
  else if constexpr (is_same_v<T, int32_t>)
    return "int32_t";
  else if constexpr (is_same_v<T, int64_t>)
    return "int64_t";
  else if constexpr (is_same_v<T, uint16_t>)
    return "uint16_t";
  else if constexpr (is_same_v<T, uint32_t>)
    return "uint32_t";
  else if constexpr (is_same_v<T, uint64_t>)
    return "uint64_t";

  // TODO: Use MAGE_UNREACHABLE once available.
  __builtin_unreachable();
}

namespace {

template <typename T> struct TypeTag {
  using type = T;
};

class RangeTest : public mage::testing::Test {
protected:
  template <typename T>
  bool expectValueEQ(T LHS, T RHS, const char *LHSStr, const char *RHSStr,
                     mage::testing::detail::Location Loc) {
    return this->test<mage::testing::TestCond::EQ>(getValueForComparison(LHS),
                                                   getValueForComparison(RHS),
                                                   LHSStr, RHSStr, Loc);
  }

  template <bool Inclusive, typename T, size_t N>
  void
  expectRangeEQ(T Begin, T End, typename range<T, Inclusive>::size_type Stride,
                const T (&Expected)[N], mage::testing::detail::Location Loc) {
    const range<T, Inclusive> Range(Begin, End, Stride);
    using size_type = typename range<T, Inclusive>::size_type;

    if (!this->test<mage::testing::TestCond::EQ>(Range.size(),
                                                 static_cast<size_type>(N),
                                                 "Range<T>.size()", "N", Loc)) {
      mage::testing::tlog() << "    Where T is: " << getTypeName<T>() << '\n';
      return;
    }

    for (size_type I = 0; I != N; ++I) {
      if (!expectValueEQ(Range[I], Expected[I], "Range<T>[I]", "Expected<T>[I]",
                         Loc)) {
        mage::testing::tlog() << "    Where T is: " << getTypeName<T>() << '\n';
        mage::testing::tlog() << "    Where I is: " << I << '\n';
      }
    }
  }
};

} // namespace

MAGE_TEST_F(RangeTest, MatchesIteratorSequence) {
  const range<int32_t> Range(-2, 2, 2);
  auto It = Range.begin();
  const auto End = Range.end();

  MAGE_EXPECT_TRUE(It != End);
  MAGE_EXPECT_FALSE(It == End);
  EXPECT_VALUE_EQ(*It, -2);

  ++It;
  MAGE_EXPECT_TRUE(It != End);
  EXPECT_VALUE_EQ(*It, 0);

  ++It;
  MAGE_EXPECT_TRUE(It != End);
  EXPECT_VALUE_EQ(*It, 2);

  ++It;
  MAGE_EXPECT_TRUE(It == End);
  MAGE_EXPECT_FALSE(It != End);
}

MAGE_TEST_F(RangeTest, MatchesIntSmallRanges) {
  auto ExpectType = [this](auto Tag) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    constexpr T InclusiveStrideOne[] = {static_cast<T>(-2), static_cast<T>(-1),
                                        static_cast<T>(0), static_cast<T>(1),
                                        static_cast<T>(2)};
    constexpr T InclusiveStrideTwo[] = {static_cast<T>(-2), static_cast<T>(0),
                                        static_cast<T>(2)};
    constexpr T ExclusiveStrideOne[] = {static_cast<T>(-2), static_cast<T>(-1),
                                        static_cast<T>(0), static_cast<T>(1)};
    constexpr T ExclusiveStrideTwo[] = {static_cast<T>(-2), static_cast<T>(0)};

    EXPECT_RANGE_EQ(true, (static_cast<T>(-2)), (static_cast<T>(2)), 1,
                    InclusiveStrideOne);
    EXPECT_RANGE_EQ(true, (static_cast<T>(-2)), (static_cast<T>(2)), 2,
                    InclusiveStrideTwo);
    EXPECT_RANGE_EQ(false, (static_cast<T>(-2)), (static_cast<T>(2)), 1,
                    ExclusiveStrideOne);
    EXPECT_RANGE_EQ(false, (static_cast<T>(-2)), (static_cast<T>(2)), 2,
                    ExclusiveStrideTwo);
  };

  ExpectType(TypeTag<int16_t>{});
  ExpectType(TypeTag<int32_t>{});
  ExpectType(TypeTag<int64_t>{});
}

MAGE_TEST_F(RangeTest, MatchesUIntSmallRanges) {
  auto ExpectType = [this](auto Tag) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    constexpr T InclusiveStrideOne[] = {static_cast<T>(0), static_cast<T>(1),
                                        static_cast<T>(2), static_cast<T>(3),
                                        static_cast<T>(4)};
    constexpr T InclusiveStrideTwo[] = {static_cast<T>(0), static_cast<T>(2),
                                        static_cast<T>(4)};
    constexpr T ExclusiveStrideOne[] = {static_cast<T>(0), static_cast<T>(1),
                                        static_cast<T>(2), static_cast<T>(3)};
    constexpr T ExclusiveStrideTwo[] = {static_cast<T>(0), static_cast<T>(2)};

    EXPECT_RANGE_EQ(true, (static_cast<T>(0)), (static_cast<T>(4)), 1,
                    InclusiveStrideOne);
    EXPECT_RANGE_EQ(true, (static_cast<T>(0)), (static_cast<T>(4)), 2,
                    InclusiveStrideTwo);
    EXPECT_RANGE_EQ(false, (static_cast<T>(0)), (static_cast<T>(4)), 1,
                    ExclusiveStrideOne);
    EXPECT_RANGE_EQ(false, (static_cast<T>(0)), (static_cast<T>(4)), 2,
                    ExclusiveStrideTwo);
  };

  ExpectType(TypeTag<uint16_t>{});
  ExpectType(TypeTag<uint32_t>{});
  ExpectType(TypeTag<uint64_t>{});
}

MAGE_TEST_F(RangeTest, MatchesFPSmallRanges) {
  auto ExpectType = [this](auto Tag, auto TrueMinValue) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T TrueMin = static_cast<T>(TrueMinValue);
    const T NegativeZero = -static_cast<T>(0.0);
    const T PositiveZero = static_cast<T>(0.0);

    const T InclusiveStrideOne[] = {-TrueMin, NegativeZero, PositiveZero,
                                    TrueMin};
    const T InclusiveStrideTwo[] = {-TrueMin, PositiveZero};
    const T ExclusiveStrideOne[] = {-TrueMin, NegativeZero, PositiveZero};
    const T ExclusiveStrideTwo[] = {-TrueMin, PositiveZero};

    EXPECT_RANGE_EQ(true, (-TrueMin), TrueMin, 1, InclusiveStrideOne);
    EXPECT_RANGE_EQ(true, (-TrueMin), TrueMin, 2, InclusiveStrideTwo);
    EXPECT_RANGE_EQ(false, (-TrueMin), TrueMin, 1, ExclusiveStrideOne);
    EXPECT_RANGE_EQ(false, (-TrueMin), TrueMin, 2, ExclusiveStrideTwo);
  };

  // TODO: Use FPInfo to get machine limits for floating point types.
  ExpectType(TypeTag<mage::float16>{}, __FLT16_DENORM_MIN__);
  ExpectType(TypeTag<float>{}, FLT_TRUE_MIN);
  ExpectType(TypeTag<double>{}, DBL_TRUE_MIN);
}

MAGE_TEST_F(RangeTest, MatchesFPFiniteRanges) {
  auto ExpectType = [this](auto Tag, auto MaxValue) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T Max = static_cast<T>(MaxValue);
    const uint64_t InclusiveSize = getFiniteSymmetricSize(Max);
    const T PreviousMax = getPreviousPositiveValue(Max);

    const range<T> InclusiveStrideOne(-Max, Max);
    EXPECT_VALUE_EQ(InclusiveStrideOne.size(), InclusiveSize);
    EXPECT_VALUE_EQ(InclusiveStrideOne[0], -Max);
    EXPECT_VALUE_EQ(InclusiveStrideOne[InclusiveStrideOne.size() - 1], Max);

    const range<T, false> ExclusiveStrideOne(-Max, Max);
    EXPECT_VALUE_EQ(ExclusiveStrideOne.size(), InclusiveSize - 1);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[0], -Max);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[ExclusiveStrideOne.size() - 1],
                    PreviousMax);

    const range<T> InclusiveStrideTwo(-Max, Max, 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo.size(), InclusiveSize / 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[0], -Max);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[InclusiveStrideTwo.size() - 1],
                    PreviousMax);

    const range<T, false> ExclusiveStrideTwo(-Max, Max, 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo.size(), InclusiveSize / 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[0], -Max);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[ExclusiveStrideTwo.size() - 1],
                    PreviousMax);
  };

  ExpectType(TypeTag<mage::float16>{}, __FLT16_MAX__);
  ExpectType(TypeTag<float>{}, FLT_MAX);
  ExpectType(TypeTag<double>{}, DBL_MAX);
}

MAGE_TEST_F(RangeTest, MatchesIntFullRanges) {
  auto ExpectType = [this](auto Tag, auto MinValue, auto MaxValue,
                           uint64_t NumValues) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T Min = static_cast<T>(MinValue);
    const T Max = static_cast<T>(MaxValue);

    const range<T> InclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(InclusiveStrideOne.size(), NumValues);
    EXPECT_VALUE_EQ(InclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideOne[InclusiveStrideOne.size() - 1], Max);

    const range<T, false> ExclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(ExclusiveStrideOne.size(), NumValues - 1);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[ExclusiveStrideOne.size() - 1],
                    static_cast<T>(Max - static_cast<T>(1)));

    const range<T> InclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[1],
                    static_cast<T>(Min + static_cast<T>(2)));
    EXPECT_VALUE_EQ(InclusiveStrideTwo[InclusiveStrideTwo.size() - 1],
                    static_cast<T>(Max - static_cast<T>(1)));

    const range<T, false> ExclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[1],
                    static_cast<T>(Min + static_cast<T>(2)));
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[ExclusiveStrideTwo.size() - 1],
                    static_cast<T>(Max - static_cast<T>(1)));
  };

  // TODO: Use FPInfo to get machine limits for integer types.
  ExpectType(TypeTag<int16_t>{}, INT16_MIN, INT16_MAX, 65536);
  ExpectType(TypeTag<int32_t>{}, INT32_MIN, INT32_MAX, 4294967296ull);
}

MAGE_TEST_F(RangeTest, MatchesUIntFullRanges) {
  auto ExpectType = [this](auto Tag, auto MaxValue, uint64_t NumValues) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T Min = static_cast<T>(0);
    const T Max = static_cast<T>(MaxValue);

    const range<T> InclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(InclusiveStrideOne.size(), NumValues);
    EXPECT_VALUE_EQ(InclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideOne[InclusiveStrideOne.size() - 1], Max);

    const range<T, false> ExclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(ExclusiveStrideOne.size(), NumValues - 1);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[ExclusiveStrideOne.size() - 1],
                    static_cast<T>(Max - static_cast<T>(1)));

    const range<T> InclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[1], static_cast<T>(2));
    EXPECT_VALUE_EQ(InclusiveStrideTwo[InclusiveStrideTwo.size() - 1],
                    static_cast<T>(Max - static_cast<T>(1)));

    const range<T, false> ExclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[1], static_cast<T>(2));
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[ExclusiveStrideTwo.size() - 1],
                    static_cast<T>(Max - static_cast<T>(1)));
  };

  ExpectType(TypeTag<uint16_t>{}, UINT16_MAX, 65536);
  ExpectType(TypeTag<uint32_t>{}, UINT32_MAX, 4294967296ull);
}

MAGE_TEST_F(RangeTest, Matches64BitBoundaryRanges) {
  const range<uint64_t, false> UIntExclusive(0, UINT64_MAX);
  EXPECT_VALUE_EQ(UIntExclusive.size(), UINT64_MAX);
  EXPECT_VALUE_EQ(UIntExclusive[0], uint64_t{0});
  EXPECT_VALUE_EQ(UIntExclusive[UIntExclusive.size() - 1], UINT64_MAX - 1);

  const range<int64_t, false> IntExclusive(INT64_MIN, INT64_MAX);
  EXPECT_VALUE_EQ(IntExclusive.size(), UINT64_MAX);
  EXPECT_VALUE_EQ(IntExclusive[0], INT64_MIN);
  EXPECT_VALUE_EQ(IntExclusive[IntExclusive.size() - 1], INT64_MAX - 1);

  const range<uint64_t> UIntInclusive(1, UINT64_MAX);
  EXPECT_VALUE_EQ(UIntInclusive.size(), UINT64_MAX);
  EXPECT_VALUE_EQ(UIntInclusive[0], uint64_t{1});
  EXPECT_VALUE_EQ(UIntInclusive[UIntInclusive.size() - 1], UINT64_MAX);
}

#undef EXPECT_RANGE_EQ
#undef EXPECT_VALUE_EQ
