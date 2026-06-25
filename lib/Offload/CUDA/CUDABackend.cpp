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

#include "mage/Offload/DeviceBuffer.hpp"
#include "mage/Support/Error.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

#include <cuda.h>

#include <assert.h>
#include <memory>
#include <mutex>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

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

[[nodiscard]] static llvm::Expected<std::string>
getDeviceName(int DeviceID, CUdevice Device) {
  char Name[256] = {};
  if (auto Err = check(cuDeviceGetName(Name, sizeof(Name), Device),
                       "error in cuDeviceGetName for device %d", DeviceID))
    return Err;

  return std::string(Name);
}

[[nodiscard]] static llvm::Expected<std::string>
getDeviceArchitecture(int DeviceID, CUdevice Device) {
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

class CUDADeviceState final : public detail::DeviceState {
public:
  ~CUDADeviceState() noexcept override {
    if (Context)
      consumeErrorWithDebugLogging(releasePrimaryContext(getID(), Device));
  }

  [[nodiscard]] static llvm::Expected<std::shared_ptr<CUDADeviceState>>
  create(int DeviceID) {
    auto DeviceOrErr = getDevice(DeviceID);
    if (!DeviceOrErr)
      return DeviceOrErr.takeError();

    CUdevice Device = *DeviceOrErr;

    auto ContextOrErr = retainPrimaryContext(DeviceID, Device);
    if (!ContextOrErr)
      return ContextOrErr.takeError();

    CUcontext Context = *ContextOrErr;

    auto NameOrErr = getDeviceName(DeviceID, Device);
    if (!NameOrErr) {
      auto Err = NameOrErr.takeError();
      if (auto ReleaseErr = releasePrimaryContext(DeviceID, Device))
        return llvm::joinErrors(std::move(Err), std::move(ReleaseErr));

      return Err;
    }

    auto ArchitectureOrErr = getDeviceArchitecture(DeviceID, Device);
    if (!ArchitectureOrErr) {
      auto Err = ArchitectureOrErr.takeError();
      if (auto ReleaseErr = releasePrimaryContext(DeviceID, Device))
        return llvm::joinErrors(std::move(Err), std::move(ReleaseErr));

      return Err;
    }

    return std::shared_ptr<CUDADeviceState>(
        new CUDADeviceState(DeviceID, Device, Context, std::move(*NameOrErr),
                            std::move(*ArchitectureOrErr)));
  }

  [[nodiscard]] CUcontext getContext() const noexcept { return Context; }

private:
  CUDADeviceState(int DeviceID, CUdevice Device, CUcontext Context,
                  std::string Name, std::string Architecture)
      : DeviceState(DeviceAPI::CUDA, DeviceID, std::move(Name),
                    std::move(Architecture)),
        Device(Device), Context(Context) {}

  CUdevice Device;
  CUcontext Context;
};

class CUDAHostBufferStorage final : public detail::HostBufferStorage {
public:
  ~CUDAHostBufferStorage() noexcept override {
    consumeErrorWithDebugLogging(freeHostBuffer());
  }

  [[nodiscard]] static llvm::Expected<
      std::shared_ptr<detail::HostBufferStorage>>
  create(std::shared_ptr<CUDADeviceState> Device, size_t SizeInBytes) {
    assert(SizeInBytes > 0 && "cannot allocate an empty host buffer");

    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    void *Data = nullptr;
    if (auto Err =
            check(cuMemHostAlloc(&Data, SizeInBytes, CU_MEMHOSTALLOC_PORTABLE),
                  "error in cuMemHostAlloc for %zu bytes on device %d",
                  SizeInBytes, Device->getID()))
      return Err;

    return std::shared_ptr<detail::HostBufferStorage>(
        new CUDAHostBufferStorage(std::move(Device), Data, SizeInBytes));
  }

private:
  CUDAHostBufferStorage(std::shared_ptr<CUDADeviceState> Device, void *Data,
                        size_t SizeInBytes) noexcept
      : HostBufferStorage(Data, SizeInBytes), Device(std::move(Device)) {}

  llvm::Error freeHostBuffer() {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(cuMemFreeHost(data()), "error in cuMemFreeHost for device %d",
                 Device->getID());
  }

  std::shared_ptr<CUDADeviceState> Device;
};

class CUDADeviceContextImpl final : public detail::DeviceContextImpl {
public:
  ~CUDADeviceContextImpl() noexcept override {
    if (Stream)
      consumeErrorWithDebugLogging(destroyStream());
  }

  [[nodiscard]] static llvm::Expected<
      std::unique_ptr<detail::DeviceContextImpl>>
  create(std::shared_ptr<CUDADeviceState> Device) {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    CUstream Stream = nullptr;
    if (auto Err =
            check(cuStreamCreate(&Stream, CU_STREAM_DEFAULT),
                  "error in cuStreamCreate for device %d", Device->getID()))
      return Err;

    return std::unique_ptr<detail::DeviceContextImpl>(
        new CUDADeviceContextImpl(std::move(Device), Stream));
  }

  [[nodiscard]] DeviceAPI getAPI() const noexcept override {
    return Device->getAPI();
  }

  [[nodiscard]] int getID() const noexcept override { return Device->getID(); }

  [[nodiscard]] llvm::StringRef getName() const override {
    return Device->getName();
  }

  [[nodiscard]] llvm::StringRef getArchitecture() const override {
    return Device->getArchitecture();
  }

  [[nodiscard]] llvm::Expected<std::pair<size_t, size_t>>
  getMemoryInfo() const override {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    size_t Free = 0;
    size_t Total = 0;
    if (auto Err =
            check(cuMemGetInfo(&Free, &Total),
                  "error in cuMemGetInfo for device %d", Device->getID()))
      return Err;

    return std::pair<size_t, size_t>(Free, Total);
  }

  [[nodiscard]] llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
  createHostBufferStorage(size_t SizeInBytes) override {
    return CUDAHostBufferStorage::create(Device, SizeInBytes);
  }

  llvm::Error synchronize() override {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(cuStreamSynchronize(Stream),
                 "error in cuStreamSynchronize for device %d", Device->getID());
  }

private:
  CUDADeviceContextImpl(std::shared_ptr<CUDADeviceState> Device,
                        CUstream Stream) noexcept
      : Device(std::move(Device)), Stream(Stream) {}

  llvm::Error destroyStream() {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err =
            check(cuStreamDestroy(Stream),
                  "error in cuStreamDestroy for device %d", Device->getID()))
      return Err;

    Stream = nullptr;
    return llvm::Error::success();
  }

  std::shared_ptr<CUDADeviceState> Device;
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
    auto DeviceOrErr = getOrCreateDeviceState(DeviceID);
    if (!DeviceOrErr)
      return DeviceOrErr.takeError();

    return CUDADeviceContextImpl::create(std::move(*DeviceOrErr));
  }

private:
  CUDABackend(int APIVersion, int DeviceCount)
      : Backend(APIVersion, DeviceCount), Devices(DeviceCount) {}

  [[nodiscard]] llvm::Expected<std::shared_ptr<CUDADeviceState>>
  getOrCreateDeviceState(int DeviceID) {
    std::lock_guard<std::mutex> Lock(DevicesMutex);

    if (auto Device = Devices[DeviceID])
      return Device;

    auto DeviceOrErr = CUDADeviceState::create(DeviceID);
    if (!DeviceOrErr)
      return DeviceOrErr.takeError();

    Devices[DeviceID] = std::move(*DeviceOrErr);
    return Devices[DeviceID];
  }

  std::mutex DevicesMutex;

  // Keep device state alive for the lifetime of the backend so CUDA primary
  // contexts are retained once and reused by all device resource instances.
  std::vector<std::shared_ptr<CUDADeviceState>> Devices;
};

} // namespace

llvm::Expected<detail::Backend &> detail::getCUDABackend() {
  return CUDABackend::get();
}
