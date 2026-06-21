//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements the HIP backend and its factory.
///
//===----------------------------------------------------------------------===//

#include "HIPBackend.hpp"

#include "Backend.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

#include <hip/hip_runtime_api.h>

#include <string>

using namespace mage;

template <typename... ArgsTy>
[[nodiscard]] static llvm::Error check(hipError_t Result,
                                       const char *ContextFmt, ArgsTy... Args) {
  if (Result == hipSuccess)
    return llvm::Error::success();

  const char *Description = nullptr;
  std::string FallbackDescription;

  if (hipDrvGetErrorString(Result, &Description) != hipSuccess) {
    FallbackDescription = llvm::formatv("unrecognized HIP error code {0}",
                                        static_cast<int>(Result))
                              .str();
    Description = FallbackDescription.c_str();
  }

  std::string Context;
  llvm::raw_string_ostream(Context) << llvm::format(ContextFmt, Args...);

  return llvm::createStringError("%s: %s", Context.c_str(), Description);
}

namespace {

class HIPBackend final : public detail::Backend {
public:
  [[nodiscard]] DeviceAPI getAPI() const override { return DeviceAPI::HIP; }

  [[nodiscard]] static llvm::Expected<Backend &> get() {
    static Backend *Instance = nullptr;

    static llvm::once_flag InitFlag;
    static hipError_t InitResult;
    static const char *InitContext = nullptr;

    llvm::call_once(InitFlag, [] {
      InitResult = hipInit(0);
      if (InitResult != hipSuccess) {
        InitContext = "error in hipInit";
        return;
      }

      int APIVersion = 0;
      InitResult = hipRuntimeGetVersion(&APIVersion);
      if (InitResult != hipSuccess) {
        InitContext = "error in hipRuntimeGetVersion";
        return;
      }

      int DeviceCount = 0;
      InitResult = hipGetDeviceCount(&DeviceCount);
      if (InitResult != hipSuccess) {
        InitContext = "error in hipGetDeviceCount";
        return;
      }

      static HIPBackend Backend(APIVersion, DeviceCount);
      Instance = &Backend;
    });

    if (auto Err = check(InitResult, InitContext))
      return Err;

    return *Instance;
  }

private:
  HIPBackend(int APIVersion, int DeviceCount)
      : Backend(APIVersion, DeviceCount) {}
};

} // namespace

llvm::Expected<detail::Backend &> detail::getHIPBackend() {
  return HIPBackend::get();
}
