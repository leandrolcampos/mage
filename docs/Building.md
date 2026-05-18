# Building Mage

This document outlines how to configure, build, and test Mage.

Mage is expected to be built with the LLVM-based toolchain described in
[Building LLVM](BuildingLLVM.md).

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
target triples. To disable GPU builds, configure with an empty value:

```bash
cmake -S . -B build -G Ninja \
  -DMAGE_LLVM_ROOT="$LLVM_ROOT" \
  -DMAGE_GPU_TARGET_TRIPLES=""
```

## 2. Common Build Targets

Build the Mage library for the host build and for all enabled GPU builds:

```bash
ninja -C build
```

This is equivalent to building the default `mage-all` target. It builds the Mage
library artifacts, but it does not build or run benchmarks, experiments, or tests
unless they are explicitly requested.

Build only the Mage library for the host build:

```bash
ninja -C build mage
```

Build only the Mage library for the AMDGPU build:

```bash
ninja -C build mage-amdgcn-amd-amdhsa
```

Build only the Mage library for the NVPTX build:

```bash
ninja -C build mage-nvptx64-nvidia-cuda
```

## 3. Configure GPU Builds Explicitly

Configure only the AMDGPU build directory:

```bash
ninja -C build configure-mage-amdgcn-amd-amdhsa
```

Configure only the NVPTX build directory:

```bash
ninja -C build configure-mage-nvptx64-nvidia-cuda
```

These targets are useful when you want to configure or refresh a GPU build
directory without immediately building the Mage library in that directory.

## 4. Run the Unit Tests

The `mage` and `mage-all` targets build the Mage library. Unit tests use
dedicated `check-mage` targets, which build the required test executables and
then run them with CTest.

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

CTest does not build test executables by itself. If the test executable has not
already been built, build its Ninja target first.

For example, in the host build:

```bash
ninja -C build <test-target>
ctest --output-on-failure --test-dir build -R "^<test-name>$"
```

For a GPU build, use the corresponding GPU build directory. For example, for
AMDGPU:

```bash
ninja -C build/amdgcn-amd-amdhsa <test-target>
ctest --output-on-failure --test-dir build/amdgcn-amd-amdhsa -R "^<test-name>$"
```

## 7. Relevant CMake Cache Variables

Mage exposes several CMake cache variables as part of its build interface. The
most relevant ones are:

* `MAGE_LLVM_ROOT`: LLVM install prefix used by Mage;
* `MAGE_FORCE_ASSERTIONS`: forces assertions in non-Debug builds;
* `MAGE_GPU_TARGET_TRIPLES`: semicolon-separated GPU target triples to build;
  this may be empty;
* `MAGE_FORCE_AMDGPU_ARCHITECTURE`: AMDGPU architecture to use when automatic
  detection is not desired or not available;
* `MAGE_FORCE_NVPTX_ARCHITECTURE`: NVPTX architecture to use when automatic
  detection is not desired or not available;
* `MAGE_GPU_TEST_PARALLELISM`: maximum number of GPU unit tests to run in
  parallel.
