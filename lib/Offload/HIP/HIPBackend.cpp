//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements the HIP backend.
///
//===----------------------------------------------------------------------===//

#include "HIPBackend.hpp"

#include "Backend.hpp"

#include "mage/Support/Error.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

#include <hip/hip_runtime_api.h>

#include <memory>
#include <string>
#include <utility>

using namespace mage;

template <typename... ArgsTy>
[[nodiscard]] static llvm::Error check(hipError_t Result, const char *ErrCtxFmt,
                                       ArgsTy... Args) {
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

  std::string ErrorContext;
  llvm::raw_string_ostream(ErrorContext) << llvm::format(ErrCtxFmt, Args...);

  return llvm::createStringError("%s: %s", ErrorContext.c_str(), Description);
}

[[nodiscard]] static llvm::Expected<hipDeviceProp_t>
getDeviceProperties(int DeviceID) {
  hipDeviceProp_t Properties = {};
  if (auto Err =
          check(hipGetDeviceProperties(&Properties, DeviceID),
                "error in hipGetDeviceProperties for device %d", DeviceID))
    return Err;
  return Properties;
}

namespace {

class CurrentDeviceGuard {
public:
  ~CurrentDeviceGuard() noexcept {
    if (IsActive)
      consumeErrorWithDebugLogging(
          check(hipSetDevice(PreviousDeviceID),
                "error in hipSetDevice while restoring "
                "the previous HIP device"));
  }

  CurrentDeviceGuard(const CurrentDeviceGuard &) = delete;
  CurrentDeviceGuard &operator=(const CurrentDeviceGuard &) = delete;

  CurrentDeviceGuard(CurrentDeviceGuard &&Other) noexcept
      : PreviousDeviceID(Other.PreviousDeviceID), IsActive(Other.IsActive) {
    Other.IsActive = false;
  }

  CurrentDeviceGuard &operator=(CurrentDeviceGuard &&Other) = delete;

  [[nodiscard]] static llvm::Expected<CurrentDeviceGuard>
  create(int TemporaryDeviceID) {
    int PreviousDeviceID = 0;
    if (auto Err =
            check(hipGetDevice(&PreviousDeviceID), "error in hipGetDevice"))
      return Err;

    if (auto Err =
            check(hipSetDevice(TemporaryDeviceID),
                  "error in hipSetDevice for device %d", TemporaryDeviceID))
      return Err;

    return CurrentDeviceGuard(PreviousDeviceID);
  }

private:
  explicit CurrentDeviceGuard(int PreviousDeviceID) noexcept
      : PreviousDeviceID(PreviousDeviceID) {}

  int PreviousDeviceID = 0;
  bool IsActive = true;
};

class HIPDeviceContextImpl final : public detail::DeviceContextImpl {
public:
  ~HIPDeviceContextImpl() noexcept override {
    if (Stream)
      consumeErrorWithDebugLogging(destroyStream());
  }

  [[nodiscard]] static llvm::Expected<
      std::unique_ptr<detail::DeviceContextImpl>>
  create(int DeviceID) {
    auto GuardOrErr = CurrentDeviceGuard::create(DeviceID);
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    hipStream_t Stream = nullptr;
    if (auto Err = check(hipStreamCreateWithFlags(&Stream, hipStreamDefault),
                         "error in hipStreamCreate for device %d", DeviceID))
      return Err;

    return std::unique_ptr<detail::DeviceContextImpl>(
        new HIPDeviceContextImpl(DeviceID, Stream));
  }

  [[nodiscard]] DeviceAPI getAPI() const noexcept override {
    return DeviceAPI::HIP;
  }

  [[nodiscard]] int getID() const noexcept override { return DeviceID; }

  [[nodiscard]] llvm::Expected<std::string> getName() const override {
    auto PropertiesOrErr = getDeviceProperties(DeviceID);
    if (!PropertiesOrErr)
      return PropertiesOrErr.takeError();

    return std::string(PropertiesOrErr->name);
  }

  [[nodiscard]] llvm::Expected<std::string> getArchitecture() const override {
    auto PropertiesOrErr = getDeviceProperties(DeviceID);
    if (!PropertiesOrErr)
      return PropertiesOrErr.takeError();

    std::string ArchName(PropertiesOrErr->gcnArchName);
    if (ArchName.empty())
      return llvm::createStringError(
          "missing HIP architecture name for device %d", DeviceID);

    return ArchName.substr(0, ArchName.find(':'));
  }

  [[nodiscard]] llvm::Expected<std::pair<size_t, size_t>>
  getMemoryInfo() const override {
    auto GuardOrErr = CurrentDeviceGuard::create(DeviceID);
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    size_t Free = 0;
    size_t Total = 0;
    if (auto Err = check(hipMemGetInfo(&Free, &Total),
                         "error in hipMemGetInfo for device %d", DeviceID))
      return Err;

    return std::pair<size_t, size_t>(Free, Total);
  }

  llvm::Error synchronize() override {
    auto GuardOrErr = CurrentDeviceGuard::create(DeviceID);
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(hipStreamSynchronize(Stream),
                 "error in hipStreamSynchronize for device %d", DeviceID);
  }

private:
  HIPDeviceContextImpl(int DeviceID, hipStream_t Stream) noexcept
      : DeviceID(DeviceID), Stream(Stream) {}

  llvm::Error destroyStream() {
    auto GuardOrErr = CurrentDeviceGuard::create(DeviceID);
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err = check(hipStreamDestroy(Stream),
                         "error in hipStreamDestroy for device %d", DeviceID))
      return Err;

    Stream = nullptr;
    return llvm::Error::success();
  }

  int DeviceID;
  hipStream_t Stream;
};

class HIPBackend final : public detail::Backend {
public:
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

  [[nodiscard]] DeviceAPI getAPI() const noexcept override {
    return DeviceAPI::HIP;
  }

  [[nodiscard]] llvm::Expected<std::unique_ptr<detail::DeviceContextImpl>>
  createDeviceContextImpl(int DeviceID) override {
    return HIPDeviceContextImpl::create(DeviceID);
  }

private:
  HIPBackend(int APIVersion, int DeviceCount) noexcept
      : Backend(APIVersion, DeviceCount) {}
};

} // namespace

llvm::Expected<detail::Backend &> detail::getHIPBackend() {
  return HIPBackend::get();
}
