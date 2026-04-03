# Mage

Mage is a research project focused on developing microarchitecture-aware and portable algorithms for elementary functions, such as logarithmic, exponential, and trigonometric functions, targeting modern GPUs.

It investigates trade-offs and derives optimization heuristics for lookup tables, polynomial evaluation, and precision extension across diverse GPU designs and under multiple accuracy profiles.

Mage also explores whether GPU-optimized implementations can generalize beyond GPUs, especially in relaxed-accuracy settings, while ensuring bitwise-identical results across architectures.

## Key Documents

- [Roadmap](docs/Roadmap.md): research questions, initial experimental scope, and tentative timeline for Mage's initial experiments.
- [Numerical Requirements and Accuracy Profiles for Portable GPU Math Functions](docs/Proposals/RequirementsAndProfiles.md): draft proposal for the numerical requirements and accuracy profiles targeted by Mage.

## Key LLVM Contributions

- [Elapsed-time measurement between GPU events in LLVM Offload](https://github.com/llvm/llvm-project/pull/186856): adds support for measuring elapsed time between GPU events across the Offload stack, including the Offload API, the plugin interface, and backend implementations for AMDGPU and CUDA. This contribution enables reliable cross-backend timing of kernel launches and other queue-submitted work, which is important for Mage's benchmarking infrastructure.

## Contributing

We are not accepting pull requests at this time.

## License

This repository is licensed under the Apache License v2.0 with LLVM Exceptions (see the LLVM [License](https://llvm.org/LICENSE.txt)).
