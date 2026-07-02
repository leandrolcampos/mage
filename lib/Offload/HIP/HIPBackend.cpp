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

#include "mage/Offload/Memory.hpp"
#include "mage/Offload/Module.hpp"
#include "mage/Support/Error.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

#include <hip/hip_runtime_api.h>

#include <assert.h>
#include <limits>
#include <memory>
#include <mutex>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

using namespace mage;

//===----------------------------------------------------------------------===//
// HIP error handling
//===----------------------------------------------------------------------===//

template <typename... ArgsTy>
static llvm::Error check(hipError_t Result, const char *ErrCtxFmt,
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

//===----------------------------------------------------------------------===//
// HIP device helpers
//===----------------------------------------------------------------------===//

static llvm::Expected<hipDeviceProp_t> getDeviceProperties(int DeviceID) {
  hipDeviceProp_t Properties = {};
  if (auto Err =
          check(hipGetDeviceProperties(&Properties, DeviceID),
                "error in hipGetDeviceProperties for device %d", DeviceID))
    return Err;
  return Properties;
}

namespace {

class [[nodiscard]] CurrentDeviceGuard {
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

  static llvm::Expected<CurrentDeviceGuard> create(int TemporaryDeviceID) {
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

//===----------------------------------------------------------------------===//
// HIP device state
//===----------------------------------------------------------------------===//

class [[nodiscard]] HIPDeviceState final : public detail::DeviceState {
public:
  static llvm::Expected<std::shared_ptr<HIPDeviceState>> create(int DeviceID) {
    auto PropertiesOrErr = getDeviceProperties(DeviceID);
    if (!PropertiesOrErr)
      return PropertiesOrErr.takeError();

    std::string Name(PropertiesOrErr->name);

    std::string Architecture(PropertiesOrErr->gcnArchName);
    Architecture = Architecture.substr(0, Architecture.find(':'));

    return std::shared_ptr<HIPDeviceState>(
        new HIPDeviceState(DeviceID, std::move(Name), std::move(Architecture)));
  }

private:
  HIPDeviceState(int DeviceID, std::string Name, std::string Architecture)
      : DeviceState(DeviceAPI::HIP, DeviceID, std::move(Name),
                    std::move(Architecture)) {}
};

//===----------------------------------------------------------------------===//
// HIP stream state
//===----------------------------------------------------------------------===//

class [[nodiscard]] HIPStreamState final : public detail::StreamState {
public:
  ~HIPStreamState() noexcept override {
    if (Stream)
      consumeErrorWithDebugLogging(synchronize());

    consumeErrorWithDebugLogging(destroyStream());
  }

  static llvm::Expected<std::shared_ptr<HIPStreamState>>
  create(std::shared_ptr<HIPDeviceState> Device) {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    hipStream_t Stream = nullptr;
    if (auto Err = check(hipStreamCreateWithFlags(&Stream, hipStreamDefault),
                         "error in hipStreamCreateWithFlags for device %d",
                         Device->getID()))
      return Err;

    return std::shared_ptr<HIPStreamState>(
        new HIPStreamState(std::move(Device), Stream));
  }

  [[nodiscard]] hipStream_t get() const noexcept {
    assert(Stream && "cannot use a destroyed HIP stream");
    return Stream;
  }

  llvm::Error synchronize() override {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(hipStreamSynchronize(get()),
                 "error in hipStreamSynchronize for device %d",
                 Device->getID());
  }

  llvm::Expected<bool> hasPendingWork() const override {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    auto Result = hipStreamQuery(get());
    if (Result == hipErrorNotReady)
      return true;
    if (Result == hipSuccess)
      return false;

    return check(Result, "error in hipStreamQuery for device %d",
                 Device->getID());
  }

private:
  HIPStreamState(std::shared_ptr<HIPDeviceState> Device,
                 hipStream_t Stream) noexcept
      : Device(std::move(Device)), Stream(Stream) {}

  llvm::Error destroyStream() {
    if (!Stream)
      return llvm::Error::success();

    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err =
            check(hipStreamDestroy(Stream),
                  "error in hipStreamDestroy for device %d", Device->getID()))
      return Err;

    Stream = nullptr;
    return llvm::Error::success();
  }

  std::shared_ptr<HIPDeviceState> Device;
  hipStream_t Stream;
};

//===----------------------------------------------------------------------===//
// HIP buffer storage
//===----------------------------------------------------------------------===//

class [[nodiscard]] HIPHostBufferStorage final
    : public detail::HostBufferStorage {
public:
  ~HIPHostBufferStorage() noexcept override {
    consumeErrorWithDebugLogging(freeHostBuffer());
  }

  static llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
  create(std::shared_ptr<HIPDeviceState> Device, size_t SizeInBytes) {
    assert(SizeInBytes > 0 && "cannot allocate an empty host buffer");

    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    void *Data = nullptr;
    if (auto Err =
            check(hipHostMalloc(&Data, SizeInBytes, hipHostMallocPortable),
                  "error in hipHostMalloc for %zu bytes on device %d",
                  SizeInBytes, Device->getID()))
      return Err;

    return std::shared_ptr<detail::HostBufferStorage>(
        new HIPHostBufferStorage(std::move(Device), Data, SizeInBytes));
  }

private:
  HIPHostBufferStorage(std::shared_ptr<HIPDeviceState> Device, void *Data,
                       size_t SizeInBytes) noexcept
      : HostBufferStorage(Data, SizeInBytes), Device(std::move(Device)) {}

  llvm::Error freeHostBuffer() {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(hipHostFree(data()), "error in hipHostFree for device %d",
                 Device->getID());
  }

  std::shared_ptr<HIPDeviceState> Device;
};

class [[nodiscard]] HIPDeviceBufferStorage final
    : public detail::DeviceBufferStorage {
public:
  ~HIPDeviceBufferStorage() noexcept override {
    consumeErrorWithDebugLogging(freeDeviceBuffer());
  }

  static llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
  create(std::shared_ptr<HIPDeviceState> Device,
         std::shared_ptr<HIPStreamState> Stream, size_t SizeInBytes) {
    assert(SizeInBytes > 0 && "cannot allocate an empty device buffer");

    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    void *Data = nullptr;
    if (auto Err = check(hipMallocAsync(&Data, SizeInBytes, Stream->get()),
                         "error in hipMallocAsync for %zu bytes on device %d",
                         SizeInBytes, Device->getID()))
      return Err;

    return std::shared_ptr<detail::DeviceBufferStorage>(
        new HIPDeviceBufferStorage(std::move(Device), std::move(Stream), Data,
                                   SizeInBytes));
  }

private:
  HIPDeviceBufferStorage(std::shared_ptr<HIPDeviceState> Device,
                         std::shared_ptr<HIPStreamState> Stream, void *Data,
                         size_t SizeInBytes) noexcept
      : DeviceBufferStorage(Data, SizeInBytes), Device(std::move(Device)),
        Stream(std::move(Stream)) {}

  llvm::Error freeDeviceBuffer() {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    return check(hipFreeAsync(data(), Stream->get()),
                 "error in hipFreeAsync for device %d", Device->getID());
  }

  std::shared_ptr<HIPDeviceState> Device;
  std::shared_ptr<HIPStreamState> Stream;
};

//===----------------------------------------------------------------------===//
// HIP module storage
//===----------------------------------------------------------------------===//

class [[nodiscard]] HIPDeviceFunctionStorage final
    : public detail::DeviceFunctionStorage {
public:
  HIPDeviceFunctionStorage(
      std::shared_ptr<const detail::DeviceModuleStorage> ModuleStorage,
      hipFunction_t Function) noexcept
      : DeviceFunctionStorage(std::move(ModuleStorage)), Function(Function) {}

private:
  [[maybe_unused]] hipFunction_t Function;
};

class [[nodiscard]] HIPDeviceModuleStorage final
    : public detail::DeviceModuleStorage {
public:
  ~HIPDeviceModuleStorage() noexcept override {
    consumeErrorWithDebugLogging(unloadModule());
  }

  static llvm::Expected<std::shared_ptr<detail::DeviceModuleStorage>>
  load(std::shared_ptr<HIPDeviceState> Device, llvm::StringRef ImagePath) {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    std::string ImagePathStorage = ImagePath.str();

    hipModule_t Module = nullptr;
    if (auto Err = check(hipModuleLoad(&Module, ImagePathStorage.c_str()),
                         "error in hipModuleLoad for device image '%s' on "
                         "device %d",
                         ImagePathStorage.c_str(), Device->getID()))
      return Err;

    return std::shared_ptr<detail::DeviceModuleStorage>(
        new HIPDeviceModuleStorage(std::move(Device), Module));
  }

  llvm::Expected<int> getFunctionCount() const override {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    unsigned int Count = 0;
    if (auto Err = check(hipModuleGetFunctionCount(&Count, Module),
                         "error in hipModuleGetFunctionCount for device %d",
                         Device->getID()))
      return Err;

    if (Count > static_cast<unsigned int>(std::numeric_limits<int>::max()))
      return llvm::createStringError(
          "HIP module function count %u exceeds int range", Count);

    return static_cast<int>(Count);
  }

  llvm::Expected<std::shared_ptr<detail::DeviceFunctionStorage>>
  getFunctionStorage(
      std::shared_ptr<const detail::DeviceModuleStorage> ModuleStorage,
      llvm::StringRef FunctionName) const override {
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    std::string FunctionNameStorage = FunctionName.str();

    hipFunction_t Function = nullptr;
    if (auto Err = check(hipModuleGetFunction(&Function, Module,
                                              FunctionNameStorage.c_str()),
                         "error in hipModuleGetFunction for function '%s' on "
                         "device %d",
                         FunctionNameStorage.c_str(), Device->getID()))
      return Err;

    return std::shared_ptr<detail::DeviceFunctionStorage>(
        new HIPDeviceFunctionStorage(std::move(ModuleStorage), Function));
  }

private:
  HIPDeviceModuleStorage(std::shared_ptr<HIPDeviceState> Device,
                         hipModule_t Module) noexcept
      : Device(std::move(Device)), Module(Module) {}

  llvm::Error unloadModule() {
    if (!Module)
      return llvm::Error::success();

    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err =
            check(hipModuleUnload(Module),
                  "error in hipModuleUnload for device %d", Device->getID()))
      return Err;

    Module = nullptr;
    return llvm::Error::success();
  }

  std::shared_ptr<HIPDeviceState> Device;
  hipModule_t Module;
};

//===----------------------------------------------------------------------===//
// HIP device context
//===----------------------------------------------------------------------===//

class [[nodiscard]] HIPDeviceContextImpl final
    : public detail::DeviceContextImpl {
public:
  ~HIPDeviceContextImpl() noexcept override {
    consumeErrorWithDebugLogging(synchronize());
  }

  static llvm::Expected<std::unique_ptr<detail::DeviceContextImpl>>
  create(std::shared_ptr<HIPDeviceState> Device) {
    auto StreamOrErr = HIPStreamState::create(Device);
    if (!StreamOrErr)
      return StreamOrErr.takeError();

    return std::unique_ptr<detail::DeviceContextImpl>(
        new HIPDeviceContextImpl(std::move(Device), std::move(*StreamOrErr)));
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
    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    size_t Free = 0;
    size_t Total = 0;
    if (auto Err =
            check(hipMemGetInfo(&Free, &Total),
                  "error in hipMemGetInfo for device %d", Device->getID()))
      return Err;

    return std::pair<size_t, size_t>(Free, Total);
  }

  llvm::Expected<std::shared_ptr<detail::HostBufferStorage>>
  createHostBufferStorage(size_t SizeInBytes) override {
    return HIPHostBufferStorage::create(Device, SizeInBytes);
  }

  llvm::Expected<std::shared_ptr<detail::DeviceBufferStorage>>
  enqueueCreateBufferStorage(size_t SizeInBytes) override {
    return HIPDeviceBufferStorage::create(Device, Stream, SizeInBytes);
  }

  llvm::Expected<std::shared_ptr<detail::DeviceModuleStorage>>
  loadModuleStorage(llvm::StringRef ImagePath) override {
    return HIPDeviceModuleStorage::load(Device, ImagePath);
  }

  llvm::Error enqueueCopyToDeviceStorage(
      std::shared_ptr<detail::DeviceBufferStorage> Dst,
      std::shared_ptr<const detail::HostBufferStorage> Src,
      size_t SizeInBytes) override {
    assert(Dst && "copy destination storage must not be null");
    assert(Src && "copy source storage must not be null");
    assert(SizeInBytes > 0 && "cannot enqueue an empty copy");

    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err = check(hipMemcpyAsync(Dst->data(), Src->data(), SizeInBytes,
                                        hipMemcpyHostToDevice, Stream->get()),
                         "error in hipMemcpyAsync from host to device for %zu "
                         "bytes on device %d",
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

    auto GuardOrErr = CurrentDeviceGuard::create(Device->getID());
    if (!GuardOrErr)
      return GuardOrErr.takeError();

    if (auto Err = check(hipMemcpyAsync(Dst->data(), Src->data(), SizeInBytes,
                                        hipMemcpyDeviceToHost, Stream->get()),
                         "error in hipMemcpyAsync from device to host for %zu "
                         "bytes on device %d",
                         SizeInBytes, Device->getID()))
      return Err;

    retainPendingResource(std::move(Dst));
    retainPendingResource(std::move(Src));
    return llvm::Error::success();
  }

  llvm::Error synchronize() override {
    if (auto Err = Stream->synchronize())
      return Err;

    if (releasePendingResources() == 0)
      return llvm::Error::success();

    // Releasing pending resources may enqueue follow-up work, such as
    // asynchronous device-memory frees.
    return Stream->synchronize();
  }

  llvm::Expected<bool> hasPendingWork() const override {
    return Stream->hasPendingWork();
  }

private:
  explicit HIPDeviceContextImpl(std::shared_ptr<HIPDeviceState> Device,
                                std::shared_ptr<HIPStreamState> Stream)
      : Device(std::move(Device)), Stream(std::move(Stream)) {}

  std::shared_ptr<HIPDeviceState> Device;
  std::shared_ptr<HIPStreamState> Stream;
};

//===----------------------------------------------------------------------===//
// HIP backend
//===----------------------------------------------------------------------===//

class [[nodiscard]] HIPBackend final : public detail::Backend {
public:
  static llvm::Expected<Backend &> get() {
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

  llvm::Expected<std::unique_ptr<detail::DeviceContextImpl>>
  createDeviceContextImpl(int DeviceID) override {
    auto DeviceOrErr = getOrCreateDeviceState(DeviceID);
    if (!DeviceOrErr)
      return DeviceOrErr.takeError();

    return HIPDeviceContextImpl::create(std::move(*DeviceOrErr));
  }

private:
  HIPBackend(int APIVersion, int DeviceCount)
      : Backend(APIVersion, DeviceCount), Devices(DeviceCount) {}

  llvm::Expected<std::shared_ptr<HIPDeviceState>>
  getOrCreateDeviceState(int DeviceID) {
    std::lock_guard<std::mutex> Lock(DevicesMutex);

    if (auto Device = Devices[DeviceID])
      return Device;

    auto DeviceOrErr = HIPDeviceState::create(DeviceID);
    if (!DeviceOrErr)
      return DeviceOrErr.takeError();

    Devices[DeviceID] = std::move(*DeviceOrErr);
    return Devices[DeviceID];
  }

  std::mutex DevicesMutex;
  std::vector<std::shared_ptr<HIPDeviceState>> Devices;
};

} // namespace

llvm::Expected<detail::Backend &> detail::getHIPBackend() {
  return HIPBackend::get();
}
