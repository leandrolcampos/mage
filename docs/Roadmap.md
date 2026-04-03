# Roadmap

- Author: Leandro A. Lacerda Campos
- Status: Working document
- Scope: Initial experiments
- Last updated: April 3, 2026

## Introduction

This document is a working roadmap for Mage's initial experiments and is expected to evolve as the experimental methodology, baselines, and priorities are refined.

## Research Questions

Given a target floating-point format, an elementary function, and a rounding mode, we would like to answer the following questions:

1. What is the cost of correct rounding on GPUs?
2. How much of this cost can we reduce by exploiting trade-offs involving precision extension, polynomial evaluation, and lookup tables?
3. [Bonus, to guide future experiments] What would be considered a "very reasonable cost" for correct rounding on GPUs?

## Scope

### Initial experimental scope

- Target format: `binary32`
- Elementary function: exponential function
- Rounding mode: round to nearest, ties to even (RTE)
- Performance baselines: NVIDIA libdevice or AMD OCML, depending on the platform
- Accuracy baselines: LLVM-libc and CORE-MATH (the `master` branch version and the `as_expf_floatonly` branch version)
- Design space: trade-offs involving precision extension, polynomial evaluation, and lookup tables

## Tentative Timeline

The milestones and deadlines below are tentative and may be revised as the experiments progress.

1. Experimental scope and protocol defined. Deadline: April 30, 2026
2. Benchmarking and testing infrastructure implemented and validated. Deadline: May 31, 2026
3. Floating-Point Expansion. Deadline: June 30, 2026
4. Multiple-Precision Arithmetic. Deadline: June 30, 2026
5. Polynomial Evaluation. Deadline: June 30, 2026
6. Ozaki Scheme. Deadline: July 31, 2026
7. Experiments on trade-offs involving precision extension, polynomial evaluation, and lookup tables. Deadline: August 31, 2026
8. Correctly rounded exponential function implementation for `binary32` under RTE, and comparison against the selected baselines using the benchmarking and testing infrastructure. Deadline: September 30, 2026
