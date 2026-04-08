# Building Mage

This document outlines how to configure, build, and test Mage.

Mage is expected to be built with the LLVM-based toolchain described in [BuildingLLVM](BuildingLLVM.md).

## 1. Configure the Build

The following command configures a standard out-of-tree Ninja build for Mage
with host, AMDGPU, and NVPTX targets enabled:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER=clang \
  -DMAGE_GPU_TARGETS="amdgcn-amd-amdhsa;nvptx64-nvidia-cuda" \
  -DCMAKE_BUILD_TYPE=Release \
  -DMAGE_ENABLE_ASSERTIONS=ON
```

## 2. Common Build Targets

Build all enabled artifacts:

```bash
ninja -C build
```

Build only the host artifacts:

```bash
ninja -C build mage
```

Build only the AMDGPU leaf build:

```bash
ninja -C build mage-amdgcn-amd-amdhsa
```

Build only the NVPTX leaf build:

```bash
ninja -C build mage-nvptx64-nvidia-cuda
```

## 3. Configure GPU Leaf Builds Explicitly

Configure only the AMDGPU leaf build:

```bash
ninja -C build configure-mage-amdgcn-amd-amdhsa
```

Configure only the NVPTX leaf build:

```bash
ninja -C build configure-mage-nvptx64-nvidia-cuda
```

These targets are useful when you want to materialize or refresh a GPU leaf
build directory without immediately building its artifacts.

## 4. Run the Unit Tests

Run the host unit tests:

```bash
ninja -C build check-mage
```

Run the AMDGPU unit tests:

```bash
ninja -C build check-mage-amdgcn-amd-amdhsa
```

Run the NVPTX unit tests:

```bash
ninja -C build check-mage-nvptx64-nvidia-cuda
```

## 5. Work Directly Inside a GPU Leaf Build

After a GPU leaf build has been configured, it can also be built and tested
directly from its own build directory.

For example, for AMDGPU:

```bash
ninja -C build/amdgcn-amd-amdhsa mage
ninja -C build/amdgcn-amd-amdhsa check-mage
```

## 6. Run a Single Unit Test

To run a single unit test in the host build, use `ctest` with a name filter:

```bash
ctest --output-on-failure --test-dir build -R "^BarTest$"
```

To run a single unit test from a GPU leaf build, invoke `ctest` from the
corresponding leaf build directory instead. For example, for AMDGPU:

```bash
ctest --output-on-failure --test-dir build/amdgcn-amd-amdhsa -R "^BarTest$"
```

## 7. Relevant CMake Cache Variables

Mage exposes several CMake cache variables as part of its build interface. The
most relevant ones are:

- `MAGE_ENABLE_ASSERTIONS`: enables code assertions;
- `MAGE_LLVM_ROOT`: LLVM install prefix used by Mage;
- `LLVM_DIR`: path to the LLVM CMake package directory used by Mage;
- `MAGE_GPU_TARGETS`: semicolon-separated GPU targets to build; this may be empty;
- `MAGE_FORCE_AMDGPU_ARCH`: forces the AMDGPU architecture used for device code;
- `MAGE_FORCE_NVPTX_ARCH`: forces the NVPTX architecture used for device code;
- `MAGE_GPU_LOADER`: program used to run GPU unit tests;
- `MAGE_GPU_LOADER_ARGS`: arguments appended to the GPU test loader;
- `MAGE_GPU_TEST_JOBS`: maximum number of GPU unit tests to run in parallel.
