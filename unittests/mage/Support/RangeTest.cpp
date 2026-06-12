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

using namespace mage;

using mage::testing::Test;
using mage::testing::TestCond;
using mage::testing::tlog;
using mage::testing::detail::Location;

#define EXPECT_VALUE_EQ(LHS, RHS)                                              \
  (void)this->expectValueEQ((LHS), (RHS), #LHS, #RHS, MAGE_TEST_LOC_())

#define EXPECT_RANGE_VALUES(Range, Expected)                                   \
  this->expectRangeValues((Range), (Expected), MAGE_TEST_LOC_())

#define EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, NumParts)             \
  this->expectStridedPartitioningMatchesRange((Range), (NumParts),             \
                                              MAGE_TEST_LOC_())

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
  return bit_cast<T>(StorageType(Bits - 1));
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

class RangeTest : public Test {
protected:
  template <typename T>
  bool expectValueEQ(T LHS, T RHS, const char *LHSStr, const char *RHSStr,
                     Location Loc) {
    return this->test<TestCond::EQ>(getValueForComparison(LHS),
                                    getValueForComparison(RHS), LHSStr, RHSStr,
                                    Loc);
  }

  template <typename T, bool Inclusive, size_t N>
  void expectRangeValues(const range<T, Inclusive> &Range,
                         const T (&Expected)[N], Location Loc) {
    using size_type = typename range<T, Inclusive>::size_type;

    if (!this->test<TestCond::EQ>(Range.size(), size_type(N), "Range.size()",
                                  "N", Loc)) {
      tlog() << "    Where T is: " << getTypeName<T>() << '\n';
      return;
    }

    for (size_type I = 0; I < N; ++I)
      if (!expectValueEQ(Range[I], Expected[I], "Range[I]", "Expected[I]", Loc))
        tlog() << "    Where T is: " << getTypeName<T>() << '\n'
               << "          I is: " << I << '\n';
  }

  template <typename T, bool Inclusive>
  void expectStridedPartitioningMatchesRange(
      const range<T, Inclusive> &Range,
      typename range<T, Inclusive>::size_type NumParts, Location Loc) {
    using size_type = typename range<T, Inclusive>::size_type;

    size_type TotalSize = 0;
    for (size_type PartIndex = 0; PartIndex < NumParts; ++PartIndex) {
      const auto Partition = Range.stridedPartition(PartIndex, NumParts);
      const size_type ExpectedSize =
          (Range.size() - 1 - PartIndex) / NumParts + 1;
      TotalSize += Partition.size();

      if (!this->test<TestCond::EQ>(Partition.size(), ExpectedSize,
                                    "Partition.size()", "ExpectedSize", Loc)) {
        tlog() << "    Where T is: " << getTypeName<T>() << '\n'
               << "  PartIndex is: " << PartIndex << '\n';
        continue;
      }

      for (size_type I = 0; I < Partition.size(); ++I) {
        const size_type RangeIndex = PartIndex + I * NumParts;
        if (!expectValueEQ(Partition[I], Range[RangeIndex], "Partition[I]",
                           "Range[RangeIndex]", Loc))
          tlog() << "    Where T is: " << getTypeName<T>() << '\n'
                 << "  PartIndex is: " << PartIndex << '\n'
                 << "          I is: " << I << '\n'
                 << " RangeIndex is: " << RangeIndex << '\n';
      }
    }

    this->test<TestCond::EQ>(TotalSize, Range.size(), "TotalSize",
                             "Range.size()", Loc);
  }
};

} // namespace

//===----------------------------------------------------------------------===//
// Compile-time evaluation
//===----------------------------------------------------------------------===//

static constexpr range<int32_t> Range(-2, 4, 2);
static_assert(Range.size() == 4);
static_assert(Range[0] == -2);
static_assert(Range[3] == 4);

static constexpr auto Partition = Range.stridedPartition(1, 2);
static_assert(Partition.size() == 2);
static_assert(Partition[0] == 0);
static_assert(Partition[1] == 4);

static constexpr range<int32_t, false> ExclusiveRange(-2, 5, 2);
static constexpr auto ExclusivePartition =
    ExclusiveRange.stridedPartition(0, 3);
static_assert(ExclusivePartition.size() == 2);
static_assert(ExclusivePartition[0] == -2);
static_assert(ExclusivePartition[1] == 4);

static constexpr range<float> FloatRange(-0.0f, 0.0f);
static_assert(FloatRange.size() == 2);
static_assert(bit_cast<uint32_t>(FloatRange[0]) == 0x80000000u);
static_assert(bit_cast<uint32_t>(FloatRange[1]) == 0);

static constexpr bool iteratorsWorkInConstantExpressions() {
  constexpr range<int32_t> Range(-2, 2, 2);
  auto It = Range.begin();

  if (*It != -2)
    return false;
  ++It;
  if (*It != 0)
    return false;
  ++It;
  if (*It != 2)
    return false;
  ++It;
  return It == Range.end();
}

static_assert(iteratorsWorkInConstantExpressions());

//===----------------------------------------------------------------------===//
// Value generation and iteration
//===----------------------------------------------------------------------===//

MAGE_TEST_F(RangeTest, IteratorsProduceIndexedValues) {
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

MAGE_TEST_F(RangeTest, ProducesExpectedSignedIntegerValues) {
  auto ExpectType = [this](auto Tag) {
    using T = typename decltype(Tag)::type;
    using InclusiveRange = range<T>;
    using ExclusiveRange = range<T, false>;
    (void)Tag;

    constexpr T InclusiveStrideOne[] = {-2, -1, 0, 1, 2};
    constexpr T InclusiveStrideTwo[] = {-2, 0, 2};
    constexpr T ExclusiveStrideOne[] = {-2, -1, 0, 1};
    constexpr T ExclusiveStrideTwo[] = {-2, 0};

    EXPECT_RANGE_VALUES(InclusiveRange(-2, 2), InclusiveStrideOne);
    EXPECT_RANGE_VALUES(InclusiveRange(-2, 2, 2), InclusiveStrideTwo);
    EXPECT_RANGE_VALUES(ExclusiveRange(-2, 2), ExclusiveStrideOne);
    EXPECT_RANGE_VALUES(ExclusiveRange(-2, 2, 2), ExclusiveStrideTwo);
  };

  ExpectType(TypeTag<int16_t>{});
  ExpectType(TypeTag<int32_t>{});
  ExpectType(TypeTag<int64_t>{});
}

MAGE_TEST_F(RangeTest, ProducesExpectedUnsignedIntegerValues) {
  auto ExpectType = [this](auto Tag) {
    using T = typename decltype(Tag)::type;
    using InclusiveRange = range<T>;
    using ExclusiveRange = range<T, false>;
    (void)Tag;

    constexpr T InclusiveStrideOne[] = {0, 1, 2, 3, 4};
    constexpr T InclusiveStrideTwo[] = {0, 2, 4};
    constexpr T ExclusiveStrideOne[] = {0, 1, 2, 3};
    constexpr T ExclusiveStrideTwo[] = {0, 2};

    EXPECT_RANGE_VALUES(InclusiveRange(0, 4), InclusiveStrideOne);
    EXPECT_RANGE_VALUES(InclusiveRange(0, 4, 2), InclusiveStrideTwo);
    EXPECT_RANGE_VALUES(ExclusiveRange(0, 4), ExclusiveStrideOne);
    EXPECT_RANGE_VALUES(ExclusiveRange(0, 4, 2), ExclusiveStrideTwo);
  };

  ExpectType(TypeTag<uint16_t>{});
  ExpectType(TypeTag<uint32_t>{});
  ExpectType(TypeTag<uint64_t>{});
}

MAGE_TEST_F(RangeTest, ProducesExpectedFloatingPointValues) {
  auto ExpectType = [this](auto Tag, auto TrueMinValue) {
    using T = typename decltype(Tag)::type;
    using InclusiveRange = range<T>;
    using ExclusiveRange = range<T, false>;
    (void)Tag;

    const T TrueMin = TrueMinValue;
    const T NegativeZero = -0.0;
    const T PositiveZero = 0.0;

    const T InclusiveStrideOne[] = {-TrueMin, NegativeZero, PositiveZero,
                                    TrueMin};
    const T InclusiveStrideTwo[] = {-TrueMin, PositiveZero};
    const T ExclusiveStrideOne[] = {-TrueMin, NegativeZero, PositiveZero};
    const T ExclusiveStrideTwo[] = {-TrueMin, PositiveZero};

    EXPECT_RANGE_VALUES(InclusiveRange(-TrueMin, TrueMin), InclusiveStrideOne);
    EXPECT_RANGE_VALUES(InclusiveRange(-TrueMin, TrueMin, 2),
                        InclusiveStrideTwo);
    EXPECT_RANGE_VALUES(ExclusiveRange(-TrueMin, TrueMin), ExclusiveStrideOne);
    EXPECT_RANGE_VALUES(ExclusiveRange(-TrueMin, TrueMin, 2),
                        ExclusiveStrideTwo);
  };

  // TODO: Use FPInfo to get machine limits for floating point types.
  ExpectType(TypeTag<mage::float16>{}, __FLT16_DENORM_MIN__);
  ExpectType(TypeTag<float>{}, FLT_TRUE_MIN);
  ExpectType(TypeTag<double>{}, DBL_TRUE_MIN);
}

//===----------------------------------------------------------------------===//
// Boundary domains
//===----------------------------------------------------------------------===//

MAGE_TEST_F(RangeTest, SupportsFiniteFloatingPointDomains) {
  auto ExpectType = [this](auto Tag, auto MaxValue) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T Max = MaxValue;
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

MAGE_TEST_F(RangeTest, SupportsFullSignedIntegerDomains) {
  auto ExpectType = [this](auto Tag, auto MinValue, auto MaxValue,
                           uint64_t NumValues) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T Min = T(MinValue);
    const T Max = T(MaxValue);

    const range<T> InclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(InclusiveStrideOne.size(), NumValues);
    EXPECT_VALUE_EQ(InclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideOne[InclusiveStrideOne.size() - 1], Max);

    const range<T, false> ExclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(ExclusiveStrideOne.size(), NumValues - 1);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[ExclusiveStrideOne.size() - 1],
                    T(Max - 1));

    const range<T> InclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[1], T(Min + 2));
    EXPECT_VALUE_EQ(InclusiveStrideTwo[InclusiveStrideTwo.size() - 1],
                    T(Max - 1));

    const range<T, false> ExclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[1], T(Min + 2));
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[ExclusiveStrideTwo.size() - 1],
                    T(Max - 1));
  };

  // TODO: Use FPInfo to get machine limits for integer types.
  ExpectType(TypeTag<int16_t>{}, INT16_MIN, INT16_MAX, 65536);
  ExpectType(TypeTag<int32_t>{}, INT32_MIN, INT32_MAX, 4294967296ull);
}

MAGE_TEST_F(RangeTest, SupportsFullUnsignedIntegerDomains) {
  auto ExpectType = [this](auto Tag, auto MaxValue, uint64_t NumValues) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T Min = 0;
    const T Max = T(MaxValue);

    const range<T> InclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(InclusiveStrideOne.size(), NumValues);
    EXPECT_VALUE_EQ(InclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideOne[InclusiveStrideOne.size() - 1], Max);

    const range<T, false> ExclusiveStrideOne(Min, Max);
    EXPECT_VALUE_EQ(ExclusiveStrideOne.size(), NumValues - 1);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideOne[ExclusiveStrideOne.size() - 1],
                    T(Max - 1));

    const range<T> InclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(InclusiveStrideTwo[1], T(2));
    EXPECT_VALUE_EQ(InclusiveStrideTwo[InclusiveStrideTwo.size() - 1],
                    T(Max - 1));

    const range<T, false> ExclusiveStrideTwo(Min, Max, 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo.size(), NumValues / 2);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[0], Min);
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[1], T(2));
    EXPECT_VALUE_EQ(ExclusiveStrideTwo[ExclusiveStrideTwo.size() - 1],
                    T(Max - 1));
  };

  ExpectType(TypeTag<uint16_t>{}, UINT16_MAX, 65536);
  ExpectType(TypeTag<uint32_t>{}, UINT32_MAX, 4294967296ull);
}

MAGE_TEST_F(RangeTest, Supports64BitBoundaryDomains) {
  const range<uint64_t, false> UIntExclusive(0, UINT64_MAX);
  EXPECT_VALUE_EQ(UIntExclusive.size(), UINT64_MAX);
  EXPECT_VALUE_EQ(UIntExclusive[0], uint64_t(0));
  EXPECT_VALUE_EQ(UIntExclusive[UIntExclusive.size() - 1], UINT64_MAX - 1);

  const range<int64_t, false> IntExclusive(INT64_MIN, INT64_MAX);
  EXPECT_VALUE_EQ(IntExclusive.size(), UINT64_MAX);
  EXPECT_VALUE_EQ(IntExclusive[0], INT64_MIN);
  EXPECT_VALUE_EQ(IntExclusive[IntExclusive.size() - 1], INT64_MAX - 1);

  const range<uint64_t> UIntInclusive(1, UINT64_MAX);
  EXPECT_VALUE_EQ(UIntInclusive.size(), UINT64_MAX);
  EXPECT_VALUE_EQ(UIntInclusive[0], uint64_t(1));
  EXPECT_VALUE_EQ(UIntInclusive[UIntInclusive.size() - 1], UINT64_MAX);

  const range<int64_t> IntInclusive(INT64_MIN + 1, INT64_MAX);
  EXPECT_VALUE_EQ(IntInclusive.size(), UINT64_MAX);
  EXPECT_VALUE_EQ(IntInclusive[0], INT64_MIN + 1);
  EXPECT_VALUE_EQ(IntInclusive[IntInclusive.size() - 1], INT64_MAX);
}

//===----------------------------------------------------------------------===//
// Strided partitioning
//===----------------------------------------------------------------------===//

MAGE_TEST_F(RangeTest, DistributesElementsAmongStridedPartitions) {
  const range<int32_t> Range(0, 9);
  const int32_t Expected0[] = {0, 3, 6, 9};
  const int32_t Expected1[] = {1, 4, 7};
  const int32_t Expected2[] = {2, 5, 8};

  EXPECT_RANGE_VALUES(Range.stridedPartition(0, 3), Expected0);
  EXPECT_RANGE_VALUES(Range.stridedPartition(1, 3), Expected1);
  EXPECT_RANGE_VALUES(Range.stridedPartition(2, 3), Expected2);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 3);
}

MAGE_TEST_F(RangeTest, StridedPartitionsPreserveOriginalStride) {
  const range<int32_t> Range(10, 20, 2);
  const int32_t Expected0[] = {10, 16};
  const int32_t Expected1[] = {12, 18};
  const int32_t Expected2[] = {14, 20};

  EXPECT_RANGE_VALUES(Range.stridedPartition(0, 3), Expected0);
  EXPECT_RANGE_VALUES(Range.stridedPartition(1, 3), Expected1);
  EXPECT_RANGE_VALUES(Range.stridedPartition(2, 3), Expected2);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 3);
}

MAGE_TEST_F(RangeTest, StridedPartitionsSupportExclusiveRanges) {
  const range<int32_t, false> Range(-4, 5, 2);
  const int32_t Expected0[] = {-4, 0, 4};
  const int32_t Expected1[] = {-2, 2};

  EXPECT_RANGE_VALUES(Range.stridedPartition(0, 2), Expected0);
  EXPECT_RANGE_VALUES(Range.stridedPartition(1, 2), Expected1);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 2);
}

MAGE_TEST_F(RangeTest, StridedPartitionsSupportBoundaryEndpoints) {
  const range<uint64_t, false> Range(UINT64_MAX - 5, UINT64_MAX, 2);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 2);
}

MAGE_TEST_F(RangeTest, StridedPartitioningWithOnePartitionPreservesRange) {
  const range<int32_t> Range(-2, 2);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 1);
}

MAGE_TEST_F(RangeTest, StridedPartitioningIntoRangeSizeProducesSingletons) {
  const range<int32_t> Range(-2, 2);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, Range.size());
}

MAGE_TEST_F(RangeTest, StridedPartitionsSupportSingletonRanges) {
  const range<int32_t> Range(42, 42);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 1);
}

MAGE_TEST_F(RangeTest, StridedPartitionsSupportNarrowIntegerRanges) {
  const range<int16_t> SignedRange(-10, 10, 3);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(SignedRange, 4);

  const range<uint16_t> UnsignedRange(2, 30, 4);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(UnsignedRange, 5);
}

MAGE_TEST_F(RangeTest, StridedPartitionsPreserveFloatingPointOrdering) {
  auto ExpectType = [this](auto Tag, auto TrueMinValue) {
    using T = typename decltype(Tag)::type;
    (void)Tag;

    const T TrueMin = TrueMinValue;
    const T NegativeZero = -0.0;
    const T PositiveZero = 0.0;
    const range<T> Range(-TrueMin, TrueMin);
    const T Expected0[] = {-TrueMin, TrueMin};
    const T Expected1[] = {NegativeZero};
    const T Expected2[] = {PositiveZero};

    EXPECT_RANGE_VALUES(Range.stridedPartition(0, 3), Expected0);
    EXPECT_RANGE_VALUES(Range.stridedPartition(1, 3), Expected1);
    EXPECT_RANGE_VALUES(Range.stridedPartition(2, 3), Expected2);
    EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 3);
  };

  ExpectType(TypeTag<mage::float16>{}, __FLT16_DENORM_MIN__);
  ExpectType(TypeTag<float>{}, FLT_TRUE_MIN);
  ExpectType(TypeTag<double>{}, DBL_TRUE_MIN);
}

MAGE_TEST_F(RangeTest, StridedPartitionsAvoidSingletonStrideOverflow) {
  constexpr uint64_t Stride = uint64_t(1) << 63;
  const range<uint64_t> Range(0, UINT64_MAX - 1, Stride);
  EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE(Range, 2);
}

#undef EXPECT_STRIDED_PARTITIONING_MATCHES_RANGE
#undef EXPECT_RANGE_VALUES
#undef EXPECT_VALUE_EQ
