//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements the CUDA backend and its factory.
///
//===----------------------------------------------------------------------===//

#include "CUDABackend.hpp"

#include "Backend.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

#include <cuda.h>

#include <string>

using namespace mage;

template <typename... ArgsTy>
[[nodiscard]] static llvm::Error check(CUresult Result, const char *ContextFmt,
                                       ArgsTy... Args) {
  if (Result == CUDA_SUCCESS)
    return llvm::Error::success();

  const char *Description = nullptr;
  std::string FallbackDescription;

  if (cuGetErrorString(Result, &Description) != CUDA_SUCCESS) {
    FallbackDescription = llvm::formatv("unrecognized CUDA error code {0}",
                                        static_cast<int>(Result))
                              .str();
    Description = FallbackDescription.c_str();
  }

  std::string Context;
  llvm::raw_string_ostream(Context) << llvm::format(ContextFmt, Args...);

  return llvm::createStringError("%s: %s", Context.c_str(), Description);
}

namespace {

class CUDABackend final : public detail::Backend {
public:
  [[nodiscard]] DeviceAPI getAPI() const override { return DeviceAPI::CUDA; }

  [[nodiscard]] static llvm::Expected<Backend &> get() {
    static Backend *Instance = nullptr;

    static llvm::once_flag InitFlag;
    static CUresult InitResult;
    static const char *InitContext = nullptr;

    llvm::call_once(InitFlag, [] {
      InitResult = cuInit(0);
      if (InitResult != CUDA_SUCCESS) {
        InitContext = "error in cuInit";
        return;
      }

      int APIVersion = 0;
      InitResult = cuDriverGetVersion(&APIVersion);
      if (InitResult != CUDA_SUCCESS) {
        InitContext = "error in cuDriverGetVersion";
        return;
      }

      int DeviceCount = 0;
      InitResult = cuDeviceGetCount(&DeviceCount);
      if (InitResult != CUDA_SUCCESS) {
        InitContext = "error in cuDeviceGetCount";
        return;
      }

      static CUDABackend Backend(APIVersion, DeviceCount);
      Instance = &Backend;
    });

    if (auto Err = check(InitResult, InitContext))
      return Err;

    return *Instance;
  }

private:
  CUDABackend(int APIVersion, int DeviceCount)
      : Backend(APIVersion, DeviceCount) {}
};

} // namespace

llvm::Expected<detail::Backend &> detail::getCUDABackend() {
  return CUDABackend::get();
}
