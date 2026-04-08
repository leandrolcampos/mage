# Developing Mage

This document summarizes the build tools, repository layout, and documentation conventions used for developing Mage.

## Build System

Mage is expected to be built with the LLVM-based toolchain described in [BuildingLLVM](BuildingLLVM.md).

Mage uses:
- **CMake** for build configuration;
- **Ninja** for build execution.

The Mage build and test interface itself is documented in [Building](Building.md).

## Repository Structure

The source tree is organized using an LLVM-like directory layout to separate public APIs, implementations, tests, benchmarks, and research artifacts:

```text
mage/
├── benchmarks/         # Executables for performance measurement
├── cmake/              # CMake modules
├── docs/               # Documentation, proposals, design notes, and roadmap
├── experiments/        # Scripts and executables for research experiments
├── include/            # Public headers
│   └── mage/           # Public Mage library interfaces
│       ├── Benchmark/  # APIs for GPU performance measurement
│       ├── GPU/        # Low-level GPU execution primitives (warp/group ops)
│       ├── Math/       # Elementary math functions
│       ├── Offload/    # APIs for managing host-device interaction
│       ├── Support/    # General utilities and abstract data types
│       └── Testing/    # APIs for GPU accuracy measurement and differential testing
├── lib/                # Mage library implementations (mirrors include/mage/)
├── test/               # Executables for accuracy measurement and differential testing
└── unittests/          # Unit tests for Mage components
````

## Documentation Conventions

Markdown files should not include manual tables of contents unless there is a specific need for them outside GitHub's rendered navigation.
