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

#include "mage/Offload/Memory.hpp"
#include "mage/Offload/Module.hpp"
#include "mage/Support/Error.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

#include <cuda.h>

#include <assert.h>
#include <limits>
#include <memory>
#include <mutex>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

using namespace mage;

//===----------------------------------------------------------------------===//
// CUDA error handling
//===----------------------------------------------------------------------===//

template <typename... ArgsTy>
static llvm::Error check(CUresult Result, const char *ErrCtxFmt,
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

//===----------------------------------------------------------------------===//
// CUDA device helpers
//===----------------------------------------------------------------------===//

static llvm::Expected<CUdevice> getDevice(int DeviceID) {
  CUdevice Device;
  if (auto Err = check(cuDeviceGet(&Device, DeviceID),
                       "error in cuDeviceGet for device %d", DeviceID))
    return Err;
  return Device;
}

static llvm::Expected<CUcontext> retainPrimaryContext(int DeviceID,
                                                      CUdevice Device) {
  CUcontext Context = nullptr;
  if (auto Err =
          check(cuDevicePrimaryCtxRetain(&Context, Device),
                "error in cuDevicePrimaryCtxRetain for device %d", DeviceID))
    return Err;

  return Context;
}

static llvm::Error releasePrimaryContext(int DeviceID, CUdevice Device) {
  return check(cuDevicePrimaryCtxRelease(Device),
               "error in cuDevicePrimaryCtxRelease for device %d", DeviceID);
}

static llvm::Expected<std::string> getDeviceName(int DeviceID,
                                                 CUdevice Device) {
  char Name[256] = {};
  if (auto Err = check(cuDeviceGetName(Name, sizeof(Name), Device),
                       "error in cuDeviceGetName for device %d", DeviceID))
    return Err;

  return std::string(Name);
}

static llvm::Expected<std::string> getDeviceArchitecture(int DeviceID,
                                                         CUdevice Device) {
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

//===----------------------------------------------------------------------===//
// CUDA pointer helpers
//===----------------------------------------------------------------------===//

[[nodiscard]] static void *toOpaqueDevicePointer(CUdeviceptr Pointer) noexcept {
  return reinterpret_cast<void *>(static_cast<uintptr_t>(Pointer));
}

[[nodiscard]] static CUdeviceptr toCUDADevicePointer(void *Pointer) noexcept {
  return static_cast<CUdeviceptr>(reinterpret_cast<uintptr_t>(Pointer));
}

[[nodiscard]] static CUdeviceptr
toCUDADevicePointer(const void *Pointer) noexcept {
  return static_cast<CUdeviceptr>(reinterpret_cast<uintptr_t>(Pointer));
}

//===----------------------------------------------------------------------===//
// CUDA context helpers
//===----------------------------------------------------------------------===//

namespace {

class [[nodiscard]] CurrentContextGuard {
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

  static llvm::Expected<CurrentContextGuard>
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

//===----------------------------------------------------------------------===//
// CUDA device state
//===----------------------------------------------------------------------===//

class [[nodiscard]] CUDADeviceState final : public detail::DeviceState {
public:
  ~CUDADeviceState() noexcept override {
    if (Context)
      consumeErrorWithDebugLogging(releasePrimaryContext(getID(), Device));
  }

  static llvm::Expected<std::shared_ptr<CUDADeviceState>> create(int DeviceID) {
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

//===----------------------------------------------------------------------===//
// CUDA stream state
//===----------------------------------------------------------------------===//

class [[nodiscard]] CUDAStreamState final : public detail::StreamState {
public:
  ~CUDAStreamState() noexcept override {
    if (Stream)
      consumeErrorWithDebugLogging(synchronize());

    consumeErrorWithDebugLogging(destroyStream());
  }

  static llvm::Expected<std::shared_ptr<CUDAStreamState>>
  create(std::shared_ptr<CUDADeviceState> Device) {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    CUstream Stream = nullptr;
    if (auto Err =
            check(cuStreamCreate(&Stream, CU_STREAM_DEFAULT),
                  "error in cuStreamCreate for device %d", Device->getID()))
      return Err;

    return std::shared_ptr<CUDAStreamState>(
        new CUDAStreamState(std::move(Device), Stream));
  }

  [[nodiscard]] CUstream get() const noexcept {
    assert(Stream && "cannot use a destroyed CUDA stream");
    return Stream;
  }

  llvm::Error synchronize() override {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(cuStreamSynchronize(get()),
                 "error in cuStreamSynchronize for device %d", Device->getID());
  }

  llvm::Expected<bool> hasPendingWork() const override {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    CUresult Result = cuStreamQuery(get());
    if (Result == CUDA_ERROR_NOT_READY)
      return true;
    if (Result == CUDA_SUCCESS)
      return false;

    return check(Result, "error in cuStreamQuery for device %d",
                 Device->getID());
  }

private:
  CUDAStreamState(std::shared_ptr<CUDADeviceState> Device,
                  CUstream Stream) noexcept
      : Device(std::move(Device)), Stream(Stream) {}

  llvm::Error destroyStream() {
    if (!Stream)
      return llvm::Error::success();

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

//===----------------------------------------------------------------------===//
// CUDA buffer storage
//===----------------------------------------------------------------------===//

class [[nodiscard]] CUDAHostBufferStorage final
    : public detail::HostBufferStorage {
public:
  ~CUDAHostBufferStorage() noexcept override {
    consumeErrorWithDebugLogging(freeHostBuffer());
  }

  static llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
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
      : HostBufferStorage(DeviceAPI::CUDA, Data, SizeInBytes),
        Device(std::move(Device)) {}

  llvm::Error freeHostBuffer() {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(cuMemFreeHost(data()), "error in cuMemFreeHost for device %d",
                 Device->getID());
  }

  std::shared_ptr<CUDADeviceState> Device;
};

class [[nodiscard]] CUDADeviceBufferStorage final
    : public detail::DeviceBufferStorage {
public:
  ~CUDADeviceBufferStorage() noexcept override {
    consumeErrorWithDebugLogging(freeDeviceBuffer());
  }

  static llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
  create(std::shared_ptr<CUDADeviceState> Device, size_t SizeInBytes) {
    assert(SizeInBytes > 0 && "cannot allocate an empty device buffer");

    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    CUdeviceptr Data = 0;
    if (auto Err = check(cuMemAlloc(&Data, SizeInBytes),
                         "error in cuMemAlloc for %zu bytes on device %d",
                         SizeInBytes, Device->getID()))
      return Err;

    return std::shared_ptr<detail::DeviceBufferStorage>(
        new CUDADeviceBufferStorage(std::move(Device), Data, SizeInBytes));
  }

private:
  CUDADeviceBufferStorage(std::shared_ptr<CUDADeviceState> Device,
                          CUdeviceptr Data, size_t SizeInBytes) noexcept
      : DeviceBufferStorage(Device->getIdentity(), toOpaqueDevicePointer(Data),
                            SizeInBytes),
        Device(std::move(Device)) {}

  llvm::Error freeDeviceBuffer() {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(cuMemFree(toCUDADevicePointer(data())),
                 "error in cuMemFree for device %d", Device->getID());
  }

  std::shared_ptr<CUDADeviceState> Device;
};

//===----------------------------------------------------------------------===//
// CUDA module storage
//===----------------------------------------------------------------------===//

class [[nodiscard]] CUDADeviceFunctionStorage final
    : public detail::DeviceFunctionStorage {
public:
  CUDADeviceFunctionStorage(
      std::shared_ptr<const detail::DeviceModuleStorage> ModuleStorage,
      CUfunction Function) noexcept
      : DeviceFunctionStorage(std::move(ModuleStorage)), Function(Function) {}

private:
  [[maybe_unused]] CUfunction Function;
};

class [[nodiscard]] CUDADeviceModuleStorage final
    : public detail::DeviceModuleStorage {
public:
  ~CUDADeviceModuleStorage() noexcept override {
    consumeErrorWithDebugLogging(unloadModule());
  }

  static llvm::Expected<std::shared_ptr<detail::DeviceModuleStorage>>
  load(std::shared_ptr<CUDADeviceState> Device, llvm::StringRef ImagePath) {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    std::string ImagePathStorage = ImagePath.str();

    CUmodule Module = nullptr;
    if (auto Err = check(cuModuleLoad(&Module, ImagePathStorage.c_str()),
                         "error in cuModuleLoad for device image '%s' on "
                         "device %d",
                         ImagePathStorage.c_str(), Device->getID()))
      return Err;

    return std::shared_ptr<detail::DeviceModuleStorage>(
        new CUDADeviceModuleStorage(std::move(Device), Module));
  }

  llvm::Expected<int> getFunctionCount() const override {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    unsigned int Count = 0;
    if (auto Err = check(cuModuleGetFunctionCount(&Count, Module),
                         "error in cuModuleGetFunctionCount for device %d",
                         Device->getID()))
      return Err;

    if (Count > static_cast<unsigned int>(std::numeric_limits<int>::max()))
      return llvm::createStringError(
          "CUDA module function count %u exceeds int range", Count);

    return static_cast<int>(Count);
  }

  llvm::Expected<std::shared_ptr<detail::DeviceFunctionStorage>>
  getFunctionStorage(
      std::shared_ptr<const detail::DeviceModuleStorage> ModuleStorage,
      llvm::StringRef FunctionName) const override {
    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    std::string FunctionNameStorage = FunctionName.str();

    CUfunction Function = nullptr;
    if (auto Err = check(
            cuModuleGetFunction(&Function, Module, FunctionNameStorage.c_str()),
            "error in cuModuleGetFunction for function '%s' on "
            "device %d",
            FunctionNameStorage.c_str(), Device->getID()))
      return Err;

    return std::shared_ptr<detail::DeviceFunctionStorage>(
        new CUDADeviceFunctionStorage(std::move(ModuleStorage), Function));
  }

private:
  CUDADeviceModuleStorage(std::shared_ptr<CUDADeviceState> Device,
                          CUmodule Module) noexcept
      : Device(std::move(Device)), Module(Module) {}

  llvm::Error unloadModule() {
    if (!Module)
      return llvm::Error::success();

    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err =
            check(cuModuleUnload(Module),
                  "error in cuModuleUnload for device %d", Device->getID()))
      return Err;

    Module = nullptr;
    return llvm::Error::success();
  }

  std::shared_ptr<CUDADeviceState> Device;
  CUmodule Module;
};

//===----------------------------------------------------------------------===//
// CUDA device context
//===----------------------------------------------------------------------===//

class [[nodiscard]] CUDADeviceContextImpl final
    : public detail::DeviceContextImpl {
public:
  ~CUDADeviceContextImpl() noexcept override {
    consumeErrorWithDebugLogging(synchronize());
  }

  static llvm::Expected<std::unique_ptr<detail::DeviceContextImpl>>
  create(std::shared_ptr<CUDADeviceState> Device) {
    auto StreamOrErr = CUDAStreamState::create(Device);
    if (!StreamOrErr)
      return StreamOrErr.takeError();

    return std::unique_ptr<detail::DeviceContextImpl>(
        new CUDADeviceContextImpl(std::move(Device), std::move(*StreamOrErr)));
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

  [[nodiscard]] std::shared_ptr<const detail::DeviceIdentity>
  getDeviceIdentity() const noexcept override {
    return Device->getIdentity();
  }

  llvm::Expected<std::pair<size_t, size_t>> getMemoryInfo() const override {
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

  llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
  createHostBufferStorage(size_t SizeInBytes) override {
    return CUDAHostBufferStorage::create(Device, SizeInBytes);
  }

  llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
  createBufferStorage(size_t SizeInBytes) override {
    return CUDADeviceBufferStorage::create(Device, SizeInBytes);
  }

  llvm::Expected<std::shared_ptr<detail::DeviceModuleStorage>>
  loadModuleStorage(llvm::StringRef ImagePath) override {
    return CUDADeviceModuleStorage::load(Device, ImagePath);
  }

  llvm::Error enqueueCopyToDeviceStorage(
      std::shared_ptr<detail::DeviceBufferStorage> Dst,
      std::shared_ptr<const detail::HostBufferStorage> Src,
      size_t SizeInBytes) override {
    assert(Dst && "copy destination storage must not be null");
    assert(Src && "copy source storage must not be null");
    assert(SizeInBytes > 0 && "cannot enqueue an empty copy");

    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err =
            check(cuMemcpyHtoDAsync(toCUDADevicePointer(Dst->data()),
                                    Src->data(), SizeInBytes, Stream->get()),
                  "error in cuMemcpyHtoDAsync for %zu bytes on "
                  "device %d",
                  SizeInBytes, Device->getID()))
      return Err;

    retainPendingResource(std::move(Dst));
    retainPendingResource(std::move(Src));
    return llvm::Error::success();
  }

  llvm::Error enqueueCopyToHostStorage(
      std::shared_ptr<detail::HostBufferStorage> Dst,
      std::shared_ptr<const detail::DeviceBufferStorage> Src,
      size_t SizeInBytes) override {
    assert(Dst && "copy destination storage must not be null");
    assert(Src && "copy source storage must not be null");
    assert(SizeInBytes > 0 && "cannot enqueue an empty copy");

    auto GuardOrErr = CurrentContextGuard::create(Device->getContext());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err = check(cuMemcpyDtoHAsync(Dst->data(),
                                           toCUDADevicePointer(Src->data()),
                                           SizeInBytes, Stream->get()),
                         "error in cuMemcpyDtoHAsync for %zu bytes on "
                         "device %d",
                         SizeInBytes, Device->getID()))
      return Err;

    retainPendingResource(std::move(Dst));
    retainPendingResource(std::move(Src));
    return llvm::Error::success();
  }

  llvm::Error synchronize() override {
    if (auto Err = Stream->synchronize())
      return Err;

    releasePendingResources();
    return llvm::Error::success();
  }

  llvm::Expected<bool> hasPendingWork() const override {
    return Stream->hasPendingWork();
  }

private:
  CUDADeviceContextImpl(std::shared_ptr<CUDADeviceState> Device,
                        std::shared_ptr<CUDAStreamState> Stream)
      : Device(std::move(Device)), Stream(std::move(Stream)) {}

  std::shared_ptr<CUDADeviceState> Device;
  std::shared_ptr<CUDAStreamState> Stream;
};

//===----------------------------------------------------------------------===//
// CUDA backend
//===----------------------------------------------------------------------===//

class [[nodiscard]] CUDABackend final : public detail::Backend {
public:
  static llvm::Expected<Backend &> get() {
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

  llvm::Expected<std::unique_ptr<detail::DeviceContextImpl>>
  createDeviceContextImpl(int DeviceID) override {
    auto DeviceOrErr = getOrCreateDeviceState(DeviceID);
    if (!DeviceOrErr)
      return DeviceOrErr.takeError();

    return CUDADeviceContextImpl::create(std::move(*DeviceOrErr));
  }

private:
  CUDABackend(int APIVersion, int DeviceCount)
      : Backend(APIVersion, DeviceCount), Devices(DeviceCount) {}

  llvm::Expected<std::shared_ptr<CUDADeviceState>>
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
