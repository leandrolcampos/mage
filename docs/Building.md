# Building Mage

This document outlines how to configure, build, and test Mage.

Mage is expected to be built with the LLVM-based toolchain described in
[BuildingLLVM](BuildingLLVM.md).

## 1. Configure the Build

The following command configures a standard out-of-tree Ninja build for Mage
with the host, AMDGPU, and NVPTX builds enabled:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMAGE_LLVM_ROOT="$LLVM_ROOT" \
  -DMAGE_FORCE_ASSERTIONS=ON \
  -DMAGE_GPU_TARGET_TRIPLES="amdgcn-amd-amdhsa;nvptx64-nvidia-cuda"
```

`MAGE_GPU_TARGET_TRIPLES` selects which GPU builds are created, using LLVM
target triples.

## 2. Common Build Targets

Build all enabled artifacts:

```bash
ninja -C build
```

Build only the host artifacts:

```bash
ninja -C build mage
```

Build only the AMDGPU build:

```bash
ninja -C build mage-amdgcn-amd-amdhsa
```

Build only the NVPTX build:

```bash
ninja -C build mage-nvptx64-nvidia-cuda
```

## 3. Configure GPU Builds Explicitly

Configure only the AMDGPU build:

```bash
ninja -C build configure-mage-amdgcn-amd-amdhsa
```

Configure only the NVPTX build:

```bash
ninja -C build configure-mage-nvptx64-nvidia-cuda
```

These targets are useful when you want to configure or refresh a GPU build
directory without immediately building its artifacts.

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

## 5. Work Directly Inside a GPU Build

After a GPU build has been configured, it can also be built and tested directly
from its own build directory.

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

To run a single unit test from a GPU build, invoke `ctest` from the corresponding
GPU build directory instead. For example, for AMDGPU:

```bash
ctest --output-on-failure --test-dir build/amdgcn-amd-amdhsa -R "^BarTest$"
```

## 7. Relevant CMake Cache Variables

Mage exposes several CMake cache variables as part of its build interface. The
most relevant ones are:

- `MAGE_LLVM_ROOT`: LLVM install prefix used by Mage;
- `MAGE_FORCE_ASSERTIONS`: forces assertions in non-Debug builds;
- `MAGE_GPU_TARGET_TRIPLES`: semicolon-separated GPU target triples to build;
  this may be empty;
- `MAGE_FORCE_AMDGPU_ARCH`: forces the AMDGPU architecture used for device code;
- `MAGE_FORCE_NVPTX_ARCH`: forces the NVPTX architecture used for device code;
- `MAGE_GPU_TEST_PARALLELISM`: maximum number of GPU unit tests to run in parallel.
