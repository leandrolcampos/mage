# Developing Mage

This document summarizes the build tools, repository layout, and documentation conventions used for developing Mage.

## Build System

Mage is expected to be built with the LLVM-based toolchain described in [BuildingLLVM](BuildingLLVM.md).

Mage uses:
- **CMake** for build configuration;
- **Ninja** for build execution.

The Mage build and test interface itself is documented in [Building](Building.md).

Each CMake configuration has one build kind:

- `HOST` compiles code for the CPU side of the offloading model. It identifies
  the execution target, not the machine running the build.
- `GPU` compiles device code for a supported GPU target.

Mage CMake rule functions use `BUILD_KINDS` to restrict a CMake target to one
or more build kinds. If omitted, the target is available in both.

## Repository Structure

The source tree is organized using an LLVM-like directory layout to separate public APIs, implementations, tests, benchmarks, and research artifacts:

```text
mage/
├── benchmarks/         # Executables for performance measurement
├── cmake/              # CMake modules
├── docs/               # Documentation, proposals, design notes, and roadmap
├── experiments/        # Scripts and executables for research experiments
├── include/            # Public headers
│   └── mage/           # Public Mage library layers and component interfaces
│       ├── Benchmark/  # APIs for GPU performance measurement
│       ├── Config/     # Compile-time configuration
│       ├── GPU/        # Low-level GPU execution primitives (warp/group ops)
│       ├── Math/       # Elementary functions and reusable numerical algorithms
│       ├── Offload/    # APIs for managing host-device interaction
│       ├── Support/    # Fundamental types, data structures, and general utilities
│       └── Testing/    # Accuracy and differential-testing infrastructure
├── lib/                # Component implementation files, organized by library layer
├── test/               # Executables for accuracy measurement and differential testing
├── tools/              # Command-line tools
└── unittests/          # Unit tests for Mage components
````

Mage public APIs are organized into library layers. Each top-level directory under `include/mage` defines a layer composed of cohesive components and establishes an architectural dependency boundary.

## Documentation Conventions

Markdown files should not include manual tables of contents unless there is a specific need for them outside GitHub's rendered navigation.
