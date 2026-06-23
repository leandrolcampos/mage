//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements the CUDA backend.
///
//===----------------------------------------------------------------------===//

#include "CUDABackend.hpp"

#include "Backend.hpp"

#include "mage/Support/Error.hpp"

#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

#include <cuda.h>

#include <memory>
#include <string>
#include <utility>

using namespace mage;

template <typename... ArgsTy>
[[nodiscard]] static llvm::Error check(CUresult Result, const char *ErrCtxFmt,
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

  std::string ErrorContext;
  llvm::raw_string_ostream(ErrorContext) << llvm::format(ErrCtxFmt, Args...);

  return llvm::createStringError("%s: %s", ErrorContext.c_str(), Description);
}

[[nodiscard]] static llvm::Expected<CUdevice> getDevice(int DeviceID) {
  CUdevice Device;
  if (auto Err = check(cuDeviceGet(&Device, DeviceID),
                       "error in cuDeviceGet for device %d", DeviceID))
    return Err;
  return Device;
}

[[nodiscard]] static llvm::Expected<CUcontext>
retainPrimaryContext(int DeviceID, CUdevice Device) {
  CUcontext Context = nullptr;
  if (auto Err =
          check(cuDevicePrimaryCtxRetain(&Context, Device),
                "error in cuDevicePrimaryCtxRetain for device %d", DeviceID))
    return Err;

  return Context;
}

[[nodiscard]] static llvm::Error releasePrimaryContext(int DeviceID,
                                                       CUdevice Device) {
  return check(cuDevicePrimaryCtxRelease(Device),
               "error in cuDevicePrimaryCtxRelease for device %d", DeviceID);
}

namespace {

class CurrentContextGuard {
public:
  ~CurrentContextGuard() noexcept {
    if (IsActive)
      consumeErrorWithDebugLogging(
          check(cuCtxSetCurrent(PreviousContext),
                "error in cuCtxSetCurrent while restoring "
                "the previous CUDA context"));
  }

  CurrentContextGuard(const CurrentContextGuard &) = delete;
  CurrentContextGuard &operator=(const CurrentContextGuard &) = delete;

  CurrentContextGuard(CurrentContextGuard &&Other) noexcept
      : PreviousContext(Other.PreviousContext), IsActive(Other.IsActive) {
    Other.IsActive = false;
  }

  CurrentContextGuard &operator=(CurrentContextGuard &&Other) = delete;

  [[nodiscard]] static llvm::Expected<CurrentContextGuard>
  create(CUcontext TemporaryContext) {
    CUcontext PreviousContext = nullptr;
    if (auto Err = check(cuCtxGetCurrent(&PreviousContext),
                         "error in cuCtxGetCurrent"))
      return Err;

    if (auto Err = check(cuCtxSetCurrent(TemporaryContext),
                         "error in cuCtxSetCurrent"))
      return Err;

    return CurrentContextGuard(PreviousContext);
  }

private:
  explicit CurrentContextGuard(CUcontext PreviousContext) noexcept
      : PreviousContext(PreviousContext) {}

  CUcontext PreviousContext = nullptr;
  bool IsActive = true;
};

class CUDADeviceContextImpl final : public detail::DeviceContextImpl {
public:
  ~CUDADeviceContextImpl() noexcept override {
    if (Stream)
      consumeErrorWithDebugLogging(destroyStream());

    if (Context)
      consumeErrorWithDebugLogging(releasePrimaryContext(DeviceID, Device));
  }

  [[nodiscard]] static llvm::Expected<
      std::unique_ptr<detail::DeviceContextImpl>>
  create(int DeviceID) {
    auto DeviceOrErr = getDevice(DeviceID);
    if (!DeviceOrErr)
      return DeviceOrErr.takeError();

    CUdevice Device = *DeviceOrErr;

    auto ContextOrErr = retainPrimaryContext(DeviceID, Device);
    if (!ContextOrErr)
      return ContextOrErr.takeError();

    CUcontext Context = *ContextOrErr;

    auto GuardOrErr = CurrentContextGuard::create(Context);
    if (!GuardOrErr) {
      auto Err = GuardOrErr.takeError();
      if (auto ReleaseErr = releasePrimaryContext(DeviceID, Device))
        return llvm::joinErrors(std::move(Err), std::move(ReleaseErr));

      return Err;
    }

    CUstream Stream = nullptr;
    if (auto Err = check(cuStreamCreate(&Stream, CU_STREAM_DEFAULT),
                         "error in cuStreamCreate for device %d", DeviceID)) {
      if (auto ReleaseErr = releasePrimaryContext(DeviceID, Device))
        return llvm::joinErrors(std::move(Err), std::move(ReleaseErr));

      return Err;
    }

    return std::unique_ptr<detail::DeviceContextImpl>(
        new CUDADeviceContextImpl(DeviceID, Device, Context, Stream));
  }

  [[nodiscard]] DeviceAPI getAPI() const noexcept override {
    return DeviceAPI::CUDA;
  }

  [[nodiscard]] int getID() const noexcept override { return DeviceID; }

  [[nodiscard]] llvm::Expected<std::string> getName() const override {
    char Name[256] = {};
    if (auto Err = check(cuDeviceGetName(Name, sizeof(Name), Device),
                         "error in cuDeviceGetName for device %d", DeviceID))
      return Err;

    return std::string(Name);
  }

  [[nodiscard]] llvm::Expected<std::string> getArchitecture() const override {
    int Major = 0;
    if (auto Err = check(
            cuDeviceGetAttribute(
                &Major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, Device),
            "error in cuDeviceGetAttribute for the compute capability major "
            "of device %d",
            DeviceID))
      return Err;

    int Minor = 0;
    if (auto Err = check(
            cuDeviceGetAttribute(
                &Minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, Device),
            "error in cuDeviceGetAttribute for the compute capability minor "
            "of device %d",
            DeviceID))
      return Err;

    return llvm::formatv("sm_{0}{1}", Major, Minor).str();
  }

  [[nodiscard]] llvm::Expected<std::pair<size_t, size_t>>
  getMemoryInfo() const override {
    auto GuardOrErr = CurrentContextGuard::create(Context);
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    size_t Free = 0;
    size_t Total = 0;
    if (auto Err = check(cuMemGetInfo(&Free, &Total),
                         "error in cuMemGetInfo for device %d", DeviceID))
      return Err;

    return std::pair<size_t, size_t>(Free, Total);
  }

  llvm::Error synchronize() override {
    auto GuardOrErr = CurrentContextGuard::create(Context);
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(cuStreamSynchronize(Stream),
                 "error in cuStreamSynchronize for device %d", DeviceID);
  }

private:
  CUDADeviceContextImpl(int DeviceID, CUdevice Device, CUcontext Context,
                        CUstream Stream) noexcept
      : DeviceID(DeviceID), Device(Device), Context(Context), Stream(Stream) {}

  llvm::Error destroyStream() {
    auto GuardOrErr = CurrentContextGuard::create(Context);
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err = check(cuStreamDestroy(Stream),
                         "error in cuStreamDestroy for device %d", DeviceID))
      return Err;

    Stream = nullptr;
    return llvm::Error::success();
  }

  int DeviceID;
  CUdevice Device;
  CUcontext Context;
  CUstream Stream;
};

class CUDABackend final : public detail::Backend {
public:
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

  [[nodiscard]] DeviceAPI getAPI() const noexcept override {
    return DeviceAPI::CUDA;
  }

  [[nodiscard]] llvm::Expected<std::unique_ptr<detail::DeviceContextImpl>>
  createDeviceContextImpl(int DeviceID) override {
    return CUDADeviceContextImpl::create(DeviceID);
  }

private:
  CUDABackend(int APIVersion, int DeviceCount) noexcept
      : Backend(APIVersion, DeviceCount) {}
};

} // namespace

llvm::Expected<detail::Backend &> detail::getCUDABackend() {
  return CUDABackend::get();
}
