//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines device function launch APIs.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_OFFLOAD_EXECUTION_HPP
#define MAGE_OFFLOAD_EXECUTION_HPP

#include "mage/Config/Target.hpp"

#if MAGE_TARGET_ARCH_IS_GPU
#error "this header is only available for host targets"
#endif

#include "mage/Offload/Context.hpp"
#include "mage/Offload/Memory.hpp"
#include "mage/Offload/Module.hpp"
#include "mage/Support/TypeTraits.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"

#include <array>
#include <assert.h>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace mage {

/// Three-dimensional shape used for grid and block dimensions.
struct Dim3 {
  uint32_t X = 1;
  uint32_t Y = 1;
  uint32_t Z = 1;
};

/// Launch configuration for a device function.
struct LaunchConfig {
  /// Number of thread blocks in the launch grid.
  Dim3 GridDim;

  /// Number of threads in each thread block.
  Dim3 BlockDim;

  /// Dynamic shared-memory size per thread block, in bytes.
  size_t DynamicSharedMemoryBytes = 0;
};

namespace detail {

// These traits intentionally follow the standard-library naming convention.
// NOLINTBEGIN(readability-identifier-naming)

template <typename ArgTy> struct launch_arg_value_type {
  using type = std::decay_t<ArgTy>;

  static constexpr bool is_supported =
      is_trivially_copyable_v<type> && !std::is_pointer<type>::value;
  static constexpr bool is_device_buffer = false;
};

template <typename ElementTy>
struct launch_arg_value_type<DeviceBuffer<ElementTy> &> {
  using type = ElementTy *;

  static constexpr bool is_supported = true;
  static constexpr bool is_device_buffer = true;
};

template <typename ElementTy>
struct launch_arg_value_type<const DeviceBuffer<ElementTy> &> {
  using type = const ElementTy *;

  static constexpr bool is_supported = true;
  static constexpr bool is_device_buffer = true;
};

template <typename ArgTy>
using launch_arg_value_type_t = typename launch_arg_value_type<ArgTy>::type;

template <typename ExpectedTy, typename ValueTy, bool ExpectedIsReference,
          bool ExpectedIsPointer, bool ValueIsPointer>
struct is_launch_value_compatible_impl
    : std::integral_constant<
          bool, !ExpectedIsReference &&
                    std::is_same<std::decay_t<ExpectedTy>, ValueTy>::value> {};

template <typename ExpectedTy, typename ValueTy, bool ExpectedIsPointer,
          bool ValueIsPointer>
struct is_launch_value_compatible_impl<ExpectedTy, ValueTy, true,
                                       ExpectedIsPointer, ValueIsPointer>
    : std::false_type {};

template <typename ExpectedTy, typename ValueTy>
struct is_launch_value_compatible_impl<ExpectedTy, ValueTy, false, true, true> {
private:
  using expected_pointee_type =
      typename std::remove_pointer<std::decay_t<ExpectedTy>>::type;
  using value_pointee_type = typename std::remove_pointer<ValueTy>::type;

  static constexpr bool base_types_match =
      std::is_same<typename std::remove_cv<expected_pointee_type>::type,
                   typename std::remove_cv<value_pointee_type>::type>::value ||
      std::is_void<typename std::remove_cv<expected_pointee_type>::type>::value;

  static constexpr bool preserves_const =
      !std::is_const<value_pointee_type>::value ||
      std::is_const<expected_pointee_type>::value;

  static constexpr bool preserves_volatile =
      !std::is_volatile<value_pointee_type>::value ||
      std::is_volatile<expected_pointee_type>::value;

public:
  static constexpr bool value =
      base_types_match && preserves_const && preserves_volatile;
};

template <typename ExpectedTy, typename ValueTy>
struct is_launch_value_compatible
    : is_launch_value_compatible_impl<
          ExpectedTy, ValueTy, std::is_reference<ExpectedTy>::value,
          std::is_pointer<std::decay_t<ExpectedTy>>::value,
          std::is_pointer<ValueTy>::value> {};

template <typename ParameterTy, typename ArgTy>
struct is_launch_arg_compatible
    : std::integral_constant<
          bool, launch_arg_value_type<ArgTy>::is_supported &&
                    is_launch_value_compatible<
                        ParameterTy, launch_arg_value_type_t<ArgTy>>::value> {};

template <bool SameListSize, typename ParameterListTy, typename ArgListTy>
struct are_launch_args_compatible_impl : std::false_type {};

template <typename... ParameterTys, typename... ArgTys>
struct are_launch_args_compatible_impl<true, type_list<ParameterTys...>,
                                       type_list<ArgTys...>>
    : std::integral_constant<
          bool,
          (is_launch_arg_compatible<ParameterTys, ArgTys>::value && ...)> {};

template <typename ParameterListTy, typename... ArgTys>
struct are_launch_args_compatible
    : are_launch_args_compatible_impl<type_list_size_v<ParameterListTy> ==
                                          sizeof...(ArgTys),
                                      ParameterListTy, type_list<ArgTys...>> {};

// NOLINTEND(readability-identifier-naming)

template <typename ValueTy> struct PreparedLaunchArg {
  ValueTy Value;
  std::shared_ptr<const void> PendingResource;
};

template <size_t... Indices, typename PrepareArgFn, typename... ArgTys>
auto prepareLaunchArgs(std::index_sequence<Indices...>,
                       PrepareArgFn &&PrepareArg, ArgTys &&...Args) {
  return std::make_tuple(PrepareArg(std::integral_constant<size_t, Indices>{},
                                    std::forward<ArgTys>(Args))...);
}

} // namespace detail

template <size_t ArgIndex, typename ArgTy>
auto DeviceContext::prepareLaunchArg(ArgTy &&Arg) const {
  using ProvidedArgTy = ArgTy &&;
  using ValueTy = detail::launch_arg_value_type_t<ProvidedArgTy>;
  using PreparedArgTy = detail::PreparedLaunchArg<ValueTy>;

  if constexpr (detail::launch_arg_value_type<
                    ProvidedArgTy>::is_device_buffer) {
    if (Arg.empty())
      return llvm::Expected<PreparedArgTy>(PreparedArgTy{Arg.data(), nullptr});

    assert(Arg.Storage && "non-empty DeviceBuffer requires storage");

    if (!ownsDeviceIdentity(Arg.Storage->getDeviceIdentity()))
      return llvm::Expected<PreparedArgTy>(
          llvm::createStringError("cannot enqueue a launch with argument %zu: "
                                  "DeviceBuffer is from another device",
                                  ArgIndex));

    return llvm::Expected<PreparedArgTy>(
        PreparedArgTy{Arg.data(), Arg.Storage});
  } else {
    return llvm::Expected<PreparedArgTy>(
        PreparedArgTy{std::forward<ArgTy>(Arg), nullptr});
  }
}

template <typename FuncTy, typename... ArgTys>
llvm::Error DeviceContext::enqueueLaunch(const DeviceFunction<FuncTy> &Function,
                                         const LaunchConfig &Config,
                                         ArgTys &&...Args) {
  using FunctionTraits = function_traits<FuncTy>;
  using ParameterListTy = typename FunctionTraits::parameter_types;

  static_assert(is_same_v<typename FunctionTraits::return_type, void>,
                "enqueueLaunch requires device functions to return void");
  static_assert(type_list_size_v<ParameterListTy> == sizeof...(ArgTys),
                "enqueueLaunch called with the wrong number of arguments");
  static_assert((!std::is_pointer<std::decay_t<ArgTys>>::value && ...),
                "enqueueLaunch does not support raw pointer arguments; "
                "pass DeviceBuffer<T> for device memory");
  static_assert(
      detail::are_launch_args_compatible<ParameterListTy, ArgTys &&...>::value,
      "enqueueLaunch argument types do not match the device function "
      "signature");

  if (!Function.Storage || !Function.OwnerIdentity)
    return llvm::createStringError(
        "cannot enqueue a launch with an unresolved device function");

  if (!ownsDeviceIdentity(Function.OwnerIdentity))
    return llvm::createStringError(
        "cannot enqueue a launch with a device function from another device");

  auto PreparedLaunchArgResults = detail::prepareLaunchArgs(
      std::index_sequence_for<ArgTys...>{},
      [&](auto ArgIndex, auto &&Arg) {
        return prepareLaunchArg<decltype(ArgIndex)::value>(
            std::forward<decltype(Arg)>(Arg));
      },
      std::forward<ArgTys>(Args)...);

  llvm::Error PreparationErr = llvm::Error::success();
  std::apply(
      [&](auto &...Results) {
        auto CheckResult = [&](auto &Result) {
          if (Result)
            return;

          if (PreparationErr) {
            llvm::consumeError(Result.takeError());
            return;
          }

          PreparationErr = Result.takeError();
        };

        (CheckResult(Results), ...);
      },
      PreparedLaunchArgResults);
  if (PreparationErr)
    return PreparationErr;

  auto PreparedLaunchArgs = std::apply(
      [](auto &...Results) { return std::make_tuple(std::move(*Results)...); },
      PreparedLaunchArgResults);

  std::vector<std::shared_ptr<const void>> PendingResources;
  PendingResources.reserve(sizeof...(ArgTys));
  std::apply(
      [&](const auto &...PreparedArgs) {
        auto AppendPendingResource = [&](const auto &PreparedArg) {
          if (PreparedArg.PendingResource)
            PendingResources.push_back(PreparedArg.PendingResource);
        };

        (AppendPendingResource(PreparedArgs), ...);
      },
      PreparedLaunchArgs);

  return std::apply(
      [&](auto &...PreparedArgs) {
        std::array<void *, sizeof...(PreparedArgs)> ArgPtrs = {
            {static_cast<void *>(&PreparedArgs.Value)...}};

        return enqueueLaunchImpl(
            Function.Storage, Config,
            llvm::MutableArrayRef<void *>(ArgPtrs.data(), ArgPtrs.size()),
            PendingResources);
      },
      PreparedLaunchArgs);
}

} // namespace mage

#endif // MAGE_OFFLOAD_EXECUTION_HPP
