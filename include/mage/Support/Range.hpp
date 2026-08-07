//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides numeric ranges over representable values.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_SUPPORT_RANGE_HPP
#define MAGE_SUPPORT_RANGE_HPP

#include "mage/Support/Bit.hpp"
#include "mage/Support/TypeTraits.hpp"

#include <assert.h>
#include <limits.h>
#include <stdint.h>

namespace mage {

template <typename T, bool Inclusive = true>
class [[nodiscard]] range // NOLINT(readability-identifier-naming)
{
  static_assert(is_same_v<T, remove_cv_t<T>>,
                "T must not be const nor volatile");
  static_assert(is_arithmetic_v<T>, "T must be an arithmetic type");
  static_assert(!is_same_v<remove_cv_t<T>, bool>, "T must not be bool");
  static_assert(sizeof(T) <= sizeof(uint64_t),
                "T must be no wider than uint64_t");

public:
  using value_type = T;
  using size_type = uint64_t;

  class [[nodiscard]] iterator // NOLINT(readability-identifier-naming)
  {
  public:
    constexpr iterator(const range &Range, size_type Index) noexcept
        : Range(&Range), Index(Index) {
      assert((Index <= Range.size()) && "Index must not exceed range size");
    }

    [[nodiscard]] constexpr value_type operator*() const noexcept {
      assert((Index < Range->size()) && "Index must be less than range size");
      return (*Range)[Index];
    }

    constexpr iterator &operator++() noexcept {
      assert((Index < Range->size()) && "Index must be less than range size");
      ++Index;
      return *this;
    }

    [[nodiscard]] constexpr bool
    operator==(const iterator &Other) const noexcept {
      assert((Range == Other.Range) &&
             "iterators must refer to the same range");
      return Range == Other.Range && Index == Other.Index;
    }

    [[nodiscard]] constexpr bool
    operator!=(const iterator &Other) const noexcept {
      return !(*this == Other);
    }

  private:
    const range *Range;
    size_type Index;
  };

  using const_iterator = iterator;

  explicit constexpr range(T Begin, T End, size_type Stride = 1) noexcept
      : MappedFirst(mapToOrderedUnsigned(Begin)),
        MappedLast(mapToOrderedUnsigned(End)), Stride(Stride) {
    assert((Stride > 0) && "Stride must be greater than zero");

    if constexpr (Inclusive) {
      assert((MappedFirst <= MappedLast) &&
             "Begin must be less than or equal to End in the range ordering");
      assert(((MappedLast - MappedFirst) < UINT64_MAX) &&
             "range is too large to index");
    } else {
      assert((MappedFirst < MappedLast) &&
             "Begin must precede End in the range ordering");
      --MappedLast;
    }
  }

  [[nodiscard]] constexpr size_type size() const noexcept {
    const size_type Distance =
        static_cast<size_type>(MappedLast) - MappedFirst + 1;
    return Distance / Stride + (Distance % Stride != 0);
  }

  [[nodiscard]] constexpr value_type
  operator[](size_type Index) const noexcept {
    assert((Index < size()) && "Index is out of range");

    const StorageType MappedValue =
        static_cast<StorageType>(MappedFirst + Index * Stride);
    return mapFromOrderedUnsigned(MappedValue);
  }

  /// Returns a range representing partition \p PartitionIndex of
  /// \p NumPartitions disjoint partitions, assigning consecutive
  /// elements of this range to consecutive partitions.
  constexpr range stridedPartition(size_type PartitionIndex,
                                   size_type NumPartitions) const noexcept {
    const size_type RangeSize = size();

    assert((NumPartitions > 0) &&
           "number of partitions must be greater than zero");
    assert((NumPartitions <= RangeSize) &&
           "number of partitions must not exceed range size");
    assert((PartitionIndex < NumPartitions) &&
           "partition index is out of range");

    const size_type LastIndex =
        PartitionIndex +
        ((RangeSize - 1 - PartitionIndex) / NumPartitions) * NumPartitions;

    const StorageType PartFirst =
        static_cast<StorageType>(MappedFirst + PartitionIndex * Stride);
    const StorageType PartLast =
        static_cast<StorageType>(MappedFirst + LastIndex * Stride);

    // A non-singleton partition contains its second element in the original
    // range, which guarantees that Stride * NumPartitions is representable.
    const size_type PartStride =
        LastIndex == PartitionIndex ? Stride : Stride * NumPartitions;

    if constexpr (Inclusive) {
      return range(mapFromOrderedUnsigned(PartFirst),
                   mapFromOrderedUnsigned(PartLast), PartStride);
    } else {
      return range(
          mapFromOrderedUnsigned(PartFirst),
          mapFromOrderedUnsigned(static_cast<StorageType>(PartLast + 1)),
          PartStride);
    }
  }

  constexpr const_iterator begin() const noexcept {
    return const_iterator(*this, 0);
  }

  constexpr const_iterator end() const noexcept {
    return const_iterator(*this, size());
  }

private:
  using StorageType = storage_type_t<T>;

  static constexpr StorageType getSignMask() noexcept {
    return StorageType(1) << (sizeof(T) * CHAR_BIT - 1);
  }

  static constexpr StorageType mapToOrderedUnsigned(T Value) noexcept {
    if constexpr (is_floating_point_v<T>) {
      // TODO: Use FPBits to get the sign mask once available.
      constexpr StorageType SignMask = getSignMask();
      // TODO: Use FPBits to access the storage representation once available.
      const StorageType Unsigned = bit_cast<StorageType>(Value);
      return (Unsigned & SignMask) ? SignMask - (Unsigned - SignMask) - 1
                                   : SignMask + Unsigned;
    } else if constexpr (is_signed_v<T>) {
      const StorageType Unsigned = bit_cast<StorageType>(Value);
      return static_cast<StorageType>(Unsigned ^ getSignMask());
    } else {
      return Value;
    }
  }

  static constexpr T mapFromOrderedUnsigned(StorageType MappedValue) noexcept {
    if constexpr (is_floating_point_v<T>) {
      const StorageType SignMask = getSignMask();
      const StorageType Unsigned = (MappedValue < SignMask)
                                       ? (SignMask - MappedValue) + SignMask - 1
                                       : MappedValue - SignMask;
      return bit_cast<T>(Unsigned);
    } else if constexpr (is_signed_v<T>) {
      return bit_cast<T>(static_cast<StorageType>(MappedValue ^ getSignMask()));
    } else {
      return MappedValue;
    }
  }

  StorageType MappedFirst;
  StorageType MappedLast;
  size_type Stride;
};

} // namespace mage

#endif // MAGE_SUPPORT_RANGE_HPP
