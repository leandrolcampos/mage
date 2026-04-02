# Numerical Requirements and Accuracy Profiles for Portable GPU Math Functions

Author: Leandro A. Lacerda Campos

Date: March 13, 2026

## Table of Contents

* [Introduction](#introduction)
* [Open Standards](#open-standards)
  * [OpenCL C](#opencl-c)
  * [WebGPU Shading Language (WGSL)](#webgpu-shading-language-wgsl)
* [GPU Platforms](#gpu-platforms)
  * [AMD ROCm](#amd-rocm)
  * [NVIDIA CUDA](#nvidia-cuda)
* [Other Math Libraries](#other-math-libraries)
  * [glibc libmvec](#glibc-libmvec)
  * [Intel SVML](#intel-svml)
  * [SLEEF](#sleef)
* [Proposed Numerical Requirements and Accuracy Profiles](#proposed-numerical-requirements-and-accuracy-profiles)
  * [A Portable GPU Math Library](#a-portable-gpu-math-library)
* [References](#references)

## Introduction

This document proposes numerical requirements and accuracy profiles for portable GPU math functions. It contains answers to questions such as: which accuracy levels should be guaranteed; whether multiple accuracy profiles are needed to represent trade-offs between accuracy and performance; which data types and rounding modes should be supported; how to handle IEEE 754 special values, subnormals, and signed zeros; what can be said about floating-point exceptions; and to what extent results should be numerically consistent across GPU platforms.

The proposed requirements and profiles are derived from three sources. First, open standards (e.g., OpenCL C and WebGPU Shading Language – WGSL) provide the most portable baseline for floating-point semantics and accuracy constraints in GPU programming models. Second, GPU platforms (e.g., AMD ROCm and NVIDIA CUDA) impose hardware and toolchain constraints and define the behavior of widely used competing device math libraries. Third, other math libraries (e.g., glibc libmvec, Intel SVML, and SLEEF) provide established practice for how optimized implementations align with the numerical requirements implied by the C and IEEE 754 standards, in addition to profile mechanisms for expressing trade-offs between accuracy and performance.

The result is a requirements-oriented foundation intended to guide the design and evaluation of math function implementations optimized for GPUs and suitable for portable libraries.

## Open Standards

Open standards define portable numerical requirements by specifying observable floating-point (FP) semantics and explicit accuracy constraints for built-in operations and math functions. This section summarizes the requirements of OpenCL C and WebGPU Shading Language (WGSL) and uses them to define what can be guaranteed across devices and toolchains without relying on vendor-specific behavior.

### OpenCL C

OpenCL (Open Computing Language) is an open standard for cross-platform parallel programming of heterogeneous systems, managed by the Khronos Group. It defines a programming model and a platform interface for executing code on a range of accelerators (CPUs, GPUs, DSPs, and FPGAs).

OpenCL C is the C-based kernel language used to write OpenCL device code. It is based on C99, with restrictions and extensions specific to the OpenCL programming model, and it specifies the required numerical semantics for compliant devices.

Section 7 of the [OpenCL C 3.0](https://registry.khronos.org/OpenCL/specs/3.0-unified/html/OpenCL_C.html) specification summarizes the numerical features derived from C99 and IEEE 754 that must be supported by conformant implementations; the main points are summarized below.

#### Profiles

The numerical compliance is conditioned by the profile supported by the implementation:

* **Full Profile:** Supports the core OpenCL specification for the reported version. In practice, this profile is commonly associated with desktop and server-class OpenCL deployments.  
* **Embedded Profile:** Supports a reduced-functionality subset of the specification defined for each OpenCL version. This profile is commonly associated with resource-constrained deployments (e.g., mobile).

#### Data Types

Only single-precision FP (`float`) is a requirement. Half- and double-precision FP (`half` and `double`, respectively) are optional features.

#### Rounding Modes

Round to nearest, ties to even (RTE) is currently the only rounding mode required for single- and double-precision operations and is therefore the default rounding mode. In the embedded profile, the minimum requirement is weaker: the implementation must support at least one rounding mode, either RTE or round to zero (RTZ), chosen by the implementation.

The default rounding mode for half-precision FP operations will be RTE if it is supported; otherwise, it will be RTZ.

In addition, only static selection of rounding mode is supported. Dynamically reconfiguring the rounding modes as specified by the IEEE 754 standard is unsupported.

The built-in math functions are specified to behave as if computed under RTE. Conversions from FP to integer type always use RTZ mode, except where the user specifically asks for another rounding mode.

#### Special, Subnormal, and Signed-Zero Values

Infinity and Not-a-Number (NaN) must be supported. Support for signaling NaN is not required.

Support for subnormal numbers is optional. Subnormals passed as input for or produced as the output of FP operations and built-in math functions may be flushed to zero.

Signed zeros (`+0` and `-0`) are supported and semantically distinct. The sign must be preserved whenever the specification prescribes results using the ± notation (e.g., `sin(+0)` is `+0` and `sin(-0)` is `-0`), except in flush-to-zero scenarios where zeros produced by flushing subnormals may have an undefined sign. Some compilation options may explicitly relax signed-zero requirements for optimization.

#### Exceptions

FP exceptions are disabled in OpenCL: it does not expose an IEEE 754 FP environment (no traps and no mechanism to query, clear, or set status flags). Nevertheless, whether and when an implementation sets FP status flags or raises FP exceptions is implementation-defined, since such behavior may exist internally but is not portable nor observable through the OpenCL API. Due to performance and portability concerns, and the impracticality of servicing precise exceptions in a vector context, such features are discouraged.

The result of a FP operation that triggers an exception must match the IEEE 754 standard for the *exceptions-not-enabled* case.

#### Accuracy

The OpenCL C specification defines the maximum error permitted for FP operations and built-in math functions, expressed in ulp (units in the last place). To illustrate these requirements, the following tables present the error tolerance for the `log` function.

**Error Tolerance for the `log` Function**

| Data Type | Function | Error Tolerance |
| :---- | :---- | :---- |
| `half` | `log` | ≤ 2 ulp for the full profile, and ≤ 3 ulp for the embedded profile |
| `float` | `log` | ≤ 3 ulp for the full profile, and ≤ 4 ulp for the embedded profile |
|  | `half_log` | ≤ 8192 ulp |
|  | `native_log` | Implementation-defined |
| `double` | `log` | ≤ 3 ulp |

Functions with the `half_` prefix are implemented with a minimum of 10-bits of accuracy, i.e. the maximum error value ≤ 8192 ulp.

Functions with the `native_` prefix may map to one or more native device instructions and will typically have better performance compared to the corresponding functions without this prefix. The accuracy (and, in some cases, the domain) of these functions is implementation-defined.

The accuracy requirements are relaxed for some single-precision functions if the unsafe math optimizations are enabled via `-cl-unsafe-math-optimizations`, which includes `-cl-no-signed-zeros`, `-cl-mad-enable`, and `-cl-denorms-are-zero`.

**Error Tolerance for the `log` Function with Unsafe Math Optimizations**

| Data Type | Function | Error Tolerance |
| :---- | :---- | :---- |
| `float` | `log` | For x in the domain [0.5, 2], the maximum absolute error is ≤ $2^{-21}$;<br>otherwise, the maximum error is ≤ 3 ulp for the full profile and ≤ 4 ulp<br>for the embedded profile |

#### Numerical Consistency

OpenCL C does not guarantee bitwise reproducibility for math functions across different devices, vendors, or toolchains. Results may vary as long as they satisfy the specification’s requirements; for `native_` functions, the maximum error (and, in some cases, the input range) is implementation-defined.

### WebGPU Shading Language (WGSL)

WebGPU is an open standard for graphics and compute acceleration on the Web. Developed in the W3C GPU for the Web community and standardized by the W3C GPU for the Web Working Group, it exposes a low-level, cross-platform API that maps to native backends such as Vulkan, Metal, and Direct3D 12.

WGSL is the shading and compute-kernel language used with WebGPU. It defines the syntax and semantics of device programs, including the numerical behavior of its scalar and vector operations.

WGSL’s numerical requirements are defined in Section 15.7 of the [WGSL](https://www.w3.org/TR/2026/CRD-WGSL-20260129/) specification. They are grounded in IEEE 754, but intentionally restrict functionality to accommodate common GPU hardware constraints and portability requirements.

#### Profiles

WebGPU does not define profiles. It aims for unified behavior across all supported devices to ensure portability, achieved via a single semantic model plus optional features negotiated at device creation time.

#### Data Types

Single-precision FP (`f32`) is a requirement. Half-precision FP (`f16`) is an optional feature. Double-precision FP (`f64`) is currently unsupported.

#### Rounding Modes

No rounding mode is specified. Implementations are permitted to round intermediate results up or down to the nearest representable FP value.

Dynamic or static reconfiguration of rounding modes is unsupported.

#### Special, Subnormal, and Signed-Zero Values

Infinity and Not-a-Number (NaN) must be supported. Support for signaling NaN is not required.

For operations covered by the FP accuracy requirements, implementations may flush subnormal inputs or outputs to zero. Other operations are required to preserve subnormal values.

Implementations may ignore the sign field of an FP zero value. That is, a zero with a positive sign may behave like a zero with a negative sign, and vice-versa.

#### Exceptions

WGSL does not expose an IEEE 754 FP environment: no FP exceptions are generated, and there is no mechanism to trap or query status flags. IEEE 754 exception conditions are instead mapped to well-defined outcomes depending on when the FP expression is evaluated, subject to the Finite Math Assumption:

* **Const- or Override-Expression:** Intermediate results are evaluated following IEEE 754 rules. If any such expression overflows or evaluates to Infinity or NaN, this becomes a creation-time error.  
* **Runtime Expression:** Implementations may assume that overflow, infinities, and NaNs do not occur. Under such an implementation, if evaluating any such expression over finite operands overflows or evaluates to Infinity or NaN as an intermediate result, the final value is indeterminate (of the target type).

#### Accuracy

The WGSL specification defines the maximum error permitted for FP operations and built-in math functions using (a) absolute error bound, (b) ulp error bound, or by inheritance (defining accuracy based on a mathematical formula of other operations).

To illustrate these requirements, the following table presents the error tolerance for the `log` function.

**Error Tolerance for the `log` Function**

| Data Type | Function | Error Tolerance |
| :---- | :---- | :---- |
| `f16` | `log` | For x in the domain [0.5, 2], the maximum absolute error is ≤ $2^{-7}$;<br>otherwise, the maximum error is ≤ 3 ulp |
| `f32` | `log` | For x in the domain [0.5, 2], the maximum absolute error is ≤ $2^{-21}$;<br>otherwise, the maximum error is ≤ 3 ulp |

#### Numerical Consistency

WGSL does not guarantee bitwise reproducibility for math functions across different backend APIs, devices, or browser implementations. Implementations may produce different results; conformance is defined by WGSL’s FP evaluation rules and, where specified, its accuracy requirements.

## GPU Platforms

GPU platforms impose hardware and toolchain constraints that define the numerical behavior of device math functions. This section summarizes the floating-point (FP) semantics of AMD ROCm and NVIDIA CUDA, detailing how their programming models implement the IEEE 754 standard. It outlines their supported data types, rounding modes, exception handling limitations, and measured accuracy bounds, highlighting the vendor-specific characteristics that portable libraries must accommodate.

### AMD ROCm

AMD ROCm (Radeon Open Compute) is an open-source software stack developed by AMD for GPU-accelerated high-performance computing and heterogeneous systems. At its core is the Heterogeneous-computing Interface for Portability (HIP), a C++ programming environment and runtime designed to enable developers to deploy portable applications across a wide range of GPU architectures.

The numerical compliance of device code is primarily dictated by the [HIP math API](https://rocm.docs.amd.com/projects/HIP/en/latest/reference/math_api.html) documentation and the underlying [Open Compute Math Library](https://github.com/ROCm/llvm-project/blob/amd-staging/amd/device-libs/doc/OCML.md) (OCML) bitcode specifications. The following summary outlines how this stack implements the IEEE 754 standard for FP arithmetic.

#### Profiles

ROCm does not define profiles. Instead, numerical features and hardware capabilities are conditioned by the device's underlying architecture and Instruction Set Architecture (ISA) generation (e.g., target IDs such as `gfx90a` or `gfx1030`).

#### Data Types

Half-, single-, and double-precision FP data types (`half`, `float`, and `double`, respectively) are fully supported across all recent CDNA and RDNA architectures.

Support for Bfloat16 (`bfloat16`) is also available across these architectures.

Support for highly specialized low-precision FP data types requires newer hardware generations. For example, FP8 FNUZ variants (`__hip_fp8_e4m3_fnuz` and `__hip_fp8_e5m2_fnuz`) require the CDNA 3 architecture, while OCP FP8 variants and sub-8-bit formats (such as `__hip_fp6_e3m2` or `__hip_fp4_e2m1`) require CDNA 4 or RDNA 4 architectures.

#### Rounding Modes

RTE is the default rounding mode.

There is no dynamic reconfiguration of rounding modes. Although AMD GPUs have hardware state that includes rounding-mode controls, this is not exposed by the HIP programming model.

In principle, a subset of basic intrinsic functions can be provided with statically selected IEEE rounding modes via specifically named device functions (e.g., `__fadd_rz` for RTZ). In the HIP AMD backend, the `_rz`, `_ru`, and `_rd` suffixed forms are only declared when `OCML_BASIC_ROUNDED_OPERATIONS` is defined. However, these entry points rely on OCML rounded-operations symbols (e.g., `__ocml_add_rtz_f32`) that OCML documents as not currently available; consequently, only RTE should be assumed to work reliably.

#### Special, Subnormal, and Signed-Zero Values

Infinity and Not-a-Number (NaN) are supported. NaN handling depends on the backend or toolchain configuration. Applications that do not require strict handling of these special values can enable device-libs controls such as `finite_only_opt` (provided via OCLC bitcode), allowing optimizations under the assumption that infinities and NaNs are never consumed or produced.

Subnormals are supported by default. Applications that do not require strict accuracy may choose to flush them to zero to improve performance. In the ROCm environment, this can be controlled via compiler options or via device-libs controls such as `daz_opt`. The exact flush-to-zero semantics may not apply uniformly across all hardware operators and intrinsic math functions.

Signed zeros (`+0` and `-0`) are supported and semantically distinct, but the equality comparison `+0 == -0` evaluates to `true`. Preservation of signed zeros is expected under default IEEE-like compilation, but it can be relaxed or entirely discarded under unsafe- or fast-math settings.

#### Exceptions

The HIP programming model does not expose a portable IEEE 754 FP exception environment to GPU kernels: there is no standard HIP API to install traps, or to dynamically set, query and clear per-thread exception status flags directly from device code.

However, recent AMD GPU generations (such as RDNA2 and CDNA2) include a 32-bit `MODE` register with per-exception enable bits, and a `TRAPSTS` (trap status) register whose `EXCP` bits are set when exceptions occur. The tracked exception set includes the five standard IEEE 754 classes (invalid operation, divide by zero, overflow, underflow, and inexact) plus two AMD-specific ones: input denormal and integer divide by zero.

By default, exception trapping is disabled at kernel entry, meaning the `MODE` exception-enable bits start cleared. Under this default behavior, execution continues when an exception occurs and only the corresponding `TRAPSTS.EXCP` bits are updated. If trapping is manually enabled, program execution stops when an exception occurs and a `SIGFPE` signal is sent to the host. For IEEE 754 compliance, `MODE` bit 8 (`DX10_CLAMP`) should be disabled and bit 9 (`IEEE`) should be enabled.

In practice, utilizing these hardware traps is highly restrictive for real workloads. Enabling traps via tools like ROCgdb requires pausing the program at the beginning of every GPU kernel thread, which introduces significant overhead and is deemed impractical. Furthermore, trapped exceptions may be reported imprecisely due to delayed exception reporting on GPUs, where the halted program counter does not exactly match the faulting instruction. Finally, once an exception is triggered, the program state is not recoverable, meaning that continuing execution after clearing the `SIGFPE` signal does not restore normal non-trapping semantics.

Because these tracking mechanisms are not part of the standard HIP API and require debugger intervention or low-level assembly instrumentation, device behavior should be specified assuming non-trapping execution. So, the result of a FP operation that triggers an exception must match the IEEE 754 standard for the *exceptions-not-enabled* case.

#### Accuracy

FP built-in arithmetic operators, as well as specific math functions backed by OCML operations documented as required to be correctly rounded, comply with the IEEE 754 standard under the default RTE rounding mode. They guarantee a maximum ulp error of zero, but compile options can relax the accuracy requirements.

For math functions in general, HIP math API documentation provides measured ulp error bounds. The ulp error is stated as the absolute value of the difference in ulp between the result of a HIP math function and the result of the corresponding function in the C++ standard library, obtained according to the RTE rounding mode. The error bounds are derived from extensive, though not exhaustive, testing. Therefore, they are not guaranteed.

The following table presents the measured error for the `log` function.

**Measured Error for the `log` Function**

| Data Type | Function | Measured Error |
| :---- | :---- | :---- |
| `float` | `logf` | ≤ 2 ulp for x in the domain $[10^{-6}, 10^6]$ |
| `float` | `__logf` | ≤ 2 ulp for x in the domain $[10^{-6}, 10^6]$ |
| `double` | `log` | ≤ 1 ulp for x in the domain $[10^{-6}, 10^6]$ |

Intrinsic math functions are faster and less accurate versions of their corresponding standard math functions. They have the same name prefixed with `__`, such as `__logf`.

Although the HIP math API documentation does not provide measured error bounds for half-precision math functions, the underlying OCML specification documents a measured error of ≤ 2 ulp for `__ocml_log_f16`.

#### Numerical Consistency

AMD ROCm does not guarantee bitwise identical results for device math functions across different GPU architectures, ISA generations, or software versions. Bitwise equivalence, when needed, is a property of a pinned toolchain configuration (compiler + OCML from the same ROCm release), fixed OCML controls, and a specific target architecture.

### NVIDIA CUDA

NVIDIA CUDA is a proprietary parallel computing platform and programming model developed by NVIDIA for general-purpose computing on its own GPUs. At its core, the CUDA C/C++ programming environment and runtime enable developers to deploy parallel applications across NVIDIA GPU architectures, though its functionality and numerical behavior are coupled with specific hardware generations and proprietary SDK versions.

The numerical compliance of device code is primarily dictated by the official [CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-programming-guide/) and the underlying [`libdevice`](https://docs.nvidia.com/cuda/libdevice-users-guide/introduction.html#introduction) math library specifications. The following summary outlines how this platform implements the IEEE 754 standard for FP arithmetic, including specific deviations inherent to its microarchitecture.

#### Profiles

CUDA does not define profiles. Instead, numerical features are conditioned by the device's Compute Capability (hardware version).

#### Data Types

Half-, single-, and double-precision FP data types (`__half`, `float`, and `double`, respectively) are supported.

Support for Bfloat16 (`__nv_bfloat16`) requires Compute Capability 8.0 or higher.

Support for quad-precision FP data type (`__float128`) requires host compiler support and Compute Capability 10.0 or higher.

#### Rounding Modes

RTE is the default rounding mode.

There is no dynamic reconfiguration of rounding modes. However, a subset of the FP arithmetic operations support multiple static IEEE rounding modes, selectable via specifically named device intrinsics functions (e.g., `__fadd_rz` for RTZ).

#### Special, Subnormal, and Signed-Zero Values

Infinity and Not-a-Number (NaN) are supported. Although signaling NaN encodings are supported, they are not signaling and are handled as quiet NaN.

FP operations may alter the bit patterns of input NaN payloads in a way that does not comply with the IEEE 754 standard.

Subnormals are supported by default. However, applications that don’t require strict accuracy may choose to avoid them to improve performance by setting the `-ftz=true` (flush-to-zero) compiler option; this option may not apply uniformly to all operators and math functions.

Signed zeros (`+0` and `-0`) are supported and semantically distinct, but `+0 == -0` evaluates to `true`.

#### Exceptions

There are no hardware-level mechanisms (such as status flags or traps) to report or detect FP exceptions.

The result of a FP operation that triggers an exception must match the IEEE 754 standard for the *exceptions-not-enabled* case.

#### Accuracy

FP built-in arithmetic operators and basic intrinsic functions (any that support static specification of the rounding mode) comply with the IEEE 754 standard. They guarantee a maximum ulp error of zero regardless of the rounding mode when applicable, but compile options can relax the accuracy requirements (e.g, `-prec-div=false`).

For math functions, CUDA documentation provides measured absolute or ulp error bounds. The ulp error is stated as the absolute value of the difference in ulp between the result returned by the CUDA library function and a correctly rounded FP result obtained according to the RTE rounding mode. The error bounds are derived from extensive, though not exhaustive, testing. Therefore, they are not guaranteed.

The following table presents the measured error for the `log` function.

**Measured Error for the `log` Function**

| Data Type | Function | Measured Error |
| :---- | :---- | :---- |
| `__nv_bfloat16` | `hlog` | ≤ 0 ulp |
| `__half` | `hlog` | ≤ 0 ulp |
| `float` | `logf` | ≤ 1 ulp |
| `float` | `__logf` | For x in the domain [0.5, 2], the maximum absolute error is ≤ $2^{-21.41}$;<br>otherwise, the maximum error is ≤ 3 ulp |
| `double` | `log` | ≤ 1 ulp |
| `__float128` | `__nv_fp128_log` | ≤ 1 ulp |

Intrinsic math functions are faster and less accurate versions of their corresponding standard math functions. They have the same name prefixed with `__`, such as `__logf`.

#### Numerical Consistency

NVIDIA CUDA does not guarantee bitwise identical results for device math functions across different GPU architectures, compute capabilities, or software versions. Bitwise equivalence is only expected under the same execution conditions.

## Other Math Libraries

Other math libraries, such as glibc libmvec, Intel SVML, and SLEEF, provide established practices for aligning optimized, vectorized implementations with the numerical requirements of the C and IEEE 754 standards. This section examines how these libraries utilize profile mechanisms to express deliberate trade-offs between performance and accuracy. It outlines their approach to floating-point (FP) semantics, detailing supported data types, rounding mode restrictions, exception handling constraints, and the mechanisms used to select accuracy variants.

### glibc libmvec

Glibc libmvec is a vector math library introduced in the GNU C Library (glibc) version 2.22 to support Single Instruction, Multiple Data (SIMD) constructs. It provides vectorized implementations of standard scalar math functions using architecture-specific SIMD ISAs, originally on `x86_64` and later also on other targets. When used, typically through compiler-driven vectorization mechanisms, it allows scalar math calls in loops to be replaced with vector entry points.

The library prioritizes performance and vectorization, which involves deliberate trade-offs regarding FP exceptions, `errno`, and some special-case behavior.

#### Profiles

Unlike Intel SVML, glibc libmvec does not expose explicit selectable accuracy profiles (such as `high`, `medium`, or `low`) via compiler options. Instead, it provides a single set of vectorized functions, without a user-visible accuracy-profile mechanism.

#### Data Types

The library provides vector variants for standard single- and double-precision FP data types (`float` and `double`).

#### Rounding Modes

Glibc libmvec documents its accuracy in the round-to-nearest, ties-to-even (RTE) mode. Official project materials state that the functions do not guarantee fully correct results in rounding modes different from RTE.

#### Special, Subnormal, and Signed-Zero Values

Special-case handling is implementation-specific. In some implementations, lanes containing Not-a-Number (NaN) values, infinities, or other exceptional inputs fall back to scalar routines. However, full C and IEEE 754 compliance regarding special values is limited due to its reliance on SIMD algorithms.

#### Exceptions

When using glibc libmvec, the vectorized functions may not raise FP exceptions as required by the C and IEEE 754 standards, and they may not change `errno` in some required cases.

#### Accuracy

According to the glibc libmvec documentation, vector versions of functions in the `x86_64` library have a maximum error of 4 ulp in RTE mode.

#### Numerical Consistency

Glibc libmvec does not document a guarantee of bitwise reproducibility. The specific vector implementation invoked, and consequently the exact numerical result, depends on the target architecture, available SIMD ISA, and glibc version.

### Intel SVML

Intel SVML (Short Vector Math Library) is a specialized, hardware-optimized math library developed by Intel to provide vectorized implementations of standard mathematical functions. Designed primarily to accelerate Single Instruction, Multiple Data (SIMD) workloads across x86 architectures, SVML is supported by several toolchains, including LLVM/Clang via specific vector library settings (`-fveclib=SVML`). When enabled, it allows the compiler to replace scalar math function calls within loops with their corresponding vectorized counterparts, significantly increasing computational throughput for data-parallel operations.

SVML provides multiple accuracy and performance variants that are selected through compiler options. Because SVML prioritizes vectorization and throughput, its overall numerical behavior involves deliberate architectural trade-offs. The following summary outlines the numerical conformance of SVML, detailing its specific behavior regarding the C and IEEE 754 standards and its inherent limitations.

#### Profiles

For each math function, there are multiple accuracy-specific SVML variants, and the Intel compiler can select the most appropriate one based on compile-time accuracy requirements that trade runtime performance against numerical accuracy.

The accuracy levels (`-fimf-precision=<arg>`), and the corresponding expected performance levels, are: 

* **High (`high`):** equivalent to `-fimf-max-error=1.0` (maximum ulp error). Baseline performance.  
* **Medium (`medium`):** equivalent to `-fimf-max-error=4.0`. Better performance. This is the default setting if the option is specified and value is omitted.  
* **Low (`low`):** equivalent to `-fimf-accuracy-bits=11` (number of correct bits) for single-precision functions and `-fimf-accuracy-bits=26` for double-precision functions. Best performance available.

The accuracy level can be applied globally or restricted to a comma-separated function list (e.g., `-fimf-precision=high:sqrtf`).

**Note:** Intel oneAPI Math Kernel Library (oneMKL) Vector Mathematics (VM) names similar accuracy levels as High Accuracy (HA), Low Accuracy (LA), and Enhanced Performance (EP), respectively.

#### Data Types

Half-, single-, and double-precision FP data types (`_Float16`, `float`, and `double`) are supported.

#### Rounding Modes

SVML functions are designed to work correctly only in the round-to-nearest, ties-to-even (RTE) rounding mode.

#### Specials, Subnormal, and Signed-Zero Values

The observed behavior for special values such as Not-a-Number (NaN) values and infinities, as well as subnormals and signed zeros, is influenced by the compiler FP model (e.g., `-fp-model`) and by denormal controls such as `-ftz` (flush-to-zero option).

#### Exceptions

When SVML is enabled (e.g., via `-fimf-use-svml=true`), the SVML functions might not accurately raise FP exceptions and do not maintain `errno`.

Since SVML functions may raise unexpected FP exceptions, a developer should be cautious about using features that enable trapping on FP exceptions.

#### Accuracy

SVML does not provide a publicly documented, per-function accuracy report. Instead, accuracy is controlled through compile options such as `-fimf-precision`, which guide the selection of SVML variants.

It is important to note that forcing SVML can cause a slight decrease in overall accuracy, because even the high-accuracy SVML variants are slightly less accurate than the corresponding scalar routines.

#### Numerical Consistency

In general, Intel SVML does not guarantee bitwise reproducibility for math functions across different binaries or architectures. Results may vary depending on compiler and library version, and on the selected math-library accuracy controls (e.g., `-fimf-precision` or `-fimf-max-error`).

However, the Intel compiler provides an option (`-fimf-arch-consistency`) intended to produce bitwise-identical results across different microarchitectural implementations of the same architecture. This guarantee applies only to a single binary, and it is not guaranteed across different architectures.

### SLEEF

SLEEF (SIMD Library for Evaluating Elementary Functions) is a high-performance vectorized math library. Its public documentation states that it implements all C99 real FP math functions in single and double precision. In current releases, the project also includes a separate quad-precision math library.

The library expresses accuracy choices through distinct function entry points. It is therefore a useful reference for API-level accuracy versus performance trade-offs and for how a high-performance math library treats special cases, FP exceptions, and numerical reproducibility.

#### Profiles

SLEEF does not define a small set of global profiles comparable to OpenCL profiles or Intel SVML compiler modes. Instead, most functions expose accuracy-specific entry points. The public documentation emphasizes 1.0-ulp and 3.5-ulp variants for many elementary functions and states that the 3.5-ulp variants are generally faster than the 1.0-ulp variants.

The project history also records the addition of faster and low accuracy functions, and the current source tree includes a naming pattern, `Sleef_fast..._u3500`, for this additional variant. Project discussion indicates that this naming is intentional: these functions are not described by a ulp bound alone, and their contract may be looser than the regular `_u10` and `_u35` families; for example, the documented bound for the added fast trigonometric functions was originally given as `max(2e-6, 3500ulp)`.

For this reason, SLEEF is best described as exposing per-function accuracy families, rather than a fixed set of named global profiles.

#### Data Types

The main SLEEF libm API covers single- and double-precision FP functions (`float` and `double`). In addition, SLEEF includes a separate quad-precision library for IEEE 754 quadruple-precision functionality.

#### Rounding Modes

SLEEF functions do not guarantee their stated accuracy under rounding modes other than round to nearest, ties to even (RTE).

#### Special, Subnormal, and Signed-Zero Values

SLEEF functions treat non-finite arguments and return non-finite values as specified in the C99 specification.

Subnormal numbers are generally handled without special handling unless otherwise noted.

Cases requiring to return negative zero are handled explicitly as part of each function’s implementation.

#### Exceptions

SLEEF functions do not set `errno` and do not raise FP exceptions.

But note that SLEEF functions return non-finite values as specified in the C99 specification.

#### Accuracy

SLEEF documents function-specific error bounds in the function names themselves. For the `log` function, the reference pages document both `_u10` and `_u35` variants for single and double precision, corresponding to 1.0 ulp and 3.5 ulp error bounds, respectively.

#### Numerical Consistency

SLEEF does not specify a cross-version guarantee of bitwise reproducibility. However, SLEEF provides (and is moving to make the default) implementations intended to return bitwise identical results across platforms by standardizing on an FMA-based strategy; platforms without hardware FMA are expected to use FMA emulation.

## Proposed Numerical Requirements and Accuracy Profiles

The preceding analysis of open standards, GPU platforms, and established vector libraries provides the foundation for defining a unified baseline for portability, correctness, and performance. The following guidelines formalize these insights into a specification.

### A Portable GPU Math Library

This section establishes the numerical requirements and accuracy profiles for the proposed library. These rules are designed to ensure predictable, cross-platform behavior while equipping developers with explicit controls to balance numerical accuracy against computational throughput.

#### Profiles

To accommodate varying workload requirements, the library shall provide distinct implementation variants for each math function, categorized into four accuracy profiles. The active profile shall be selected at compile time (e.g., via preprocessor macros or template parameters), allowing the library to resolve the implementation that maximizes performance while satisfying the established error bounds.

* `correct`: Implementations guaranteeing correctly rounded results. This profile anticipates the requirements expected in the 2029 revision of the IEEE 754 standard, prioritizing strict numerical accuracy over performance.  
* `high`: Implementations guaranteeing a maximum error of ≤ 1 ulp. This serves as the baseline performance profile. Operationally, these variants may be derived from the correct profile by bypassing the accurate path and the rounding test.  
* `medium`: Implementations guaranteeing a maximum error of ≤ 4 ulp, or the maximum error bound established by the OpenCL C Full Profile for the corresponding function and data type, whichever is stricter. Consequently, this profile inherently guarantees conformance to both OpenCL C and WGSL (WebGPU) standards. This profile provides an optimized balance between accuracy and performance.  
* `low`: Implementations maximizing performance by guaranteeing correctness only for the upper half of the mantissa bits (e.g., 11 correct bits for single-precision and 26 correct bits for double-precision). This profile delivers the best performance available.

#### Data Types

The library shall support half-, single-, and double-precision floating-point (FP) data types (`float16`, `float`, and `double`, respectively), as well as Bfloat16 (`bfloat16`).

#### Rounding Modes

All implementations shall be designed and validated to meet their specified accuracy profiles strictly under the round-to-nearest, ties-to-even (RTE) rounding mode. The library is not required to guarantee error bounds under any other dynamically or statically configured rounding modes.

#### Special, Subnormal, and Signed-Zero Values

The handling of special values shall align with the IEEE 754 standard under default compilation settings, as follows:

* Infinity and Not-a-Number (NaN) shall be supported. Signaling NaNs shall be treated as quiet NaNs.  
* Subnormal numbers shall be supported and evaluated by default.  
* Signed zeros (`+0` and `-0`) shall be distinct and preserved, although the equality comparison `+0 == -0` shall evaluate to `true`.

However, if the user explicitly applies unsafe math optimizations (e.g., `fno-honor-infinities`, `fno-honor-nans`, or `fno-signed-zeros`), the library shall honor the developer's intent to trade strict edge-case handling for execution speed. Under these conditions, implementations may elide branches that preserve signed zeros or handle subnormal inputs, and strict adherence to the active profile's accuracy bounds shall no longer be guaranteed.

#### Exceptions

The library shall not use hardware-level mechanisms (such as FP status flags or traps) or the C standard `errno` variable to report or detect FP exceptions.

The result of any FP operation that triggers an exception shall strictly comply with the IEEE 754 specification for the *exceptions-not-enabled* case, returning the appropriate NaN, Infinity, or zero values silently.

#### Accuracy

The accuracy of each math function shall be controlled by the selection of an accuracy profile. This selection shall be configurable both globally (e.g., via preprocessor macros) and individually at the function level (e.g., via template parameters).

Under default compilation settings, the explicit error bounds guaranteed by the selected profile shall be strictly enforced. However, if the user explicitly applies unsafe math optimizations, the strict guarantee of conformance to these accuracy requirements is waived. In such cases, the library shall respect the user's intent to trade strict accuracy for performance. Consequently, the implementation associated with the selected profile should adapt to maximize throughput (e.g., by bypassing strict edge-case handling).

#### Numerical Consistency

For the `correct` profile, bitwise reproducibility of math functions shall be guaranteed across all supported GPU platforms, provided no unsafe math optimizations are applied.

For the `high`, `medium`, and `low` profiles, bitwise reproducibility across all supported GPU platforms shall be guaranteed strictly under identical library version and accuracy profile, provided no unsafe math optimizations are applied.

## References

[1] Khronos OpenCL Working Group, “The OpenCL™ C Specification,” Version v3.0.19, Thu, 10 Jul 2025 11:00:00 +0000, §7 “OpenCL Numerical Compliance”. URL: [[link](https://registry.khronos.org/OpenCL/specs/3.0-unified/html/OpenCL_C.html)] (accessed 2026-02-09).

[2] W3C, “WebGPU Shading Language (WGSL),” W3C Candidate Recommendation Draft, 29 January 2026, §15.7 “Floating Point Evaluation”. URL: [[link](https://www.w3.org/TR/2026/CRD-WGSL-20260129/)] (accessed 2026-02-10).

[3] NVIDIA, “CUDA Programming Guide,” v13.1, §5.5 “Floating-Point Computation”. URL: [[link](https://docs.nvidia.com/cuda/cuda-programming-guide/)] (accessed 2026-02-11).

[4] X. Li, I. Laguna, B. Fang, K. Swirydowicz, A. Li, and G. Gopalakrishnan, “Design and Evaluation of GPU-FPX: A Low-Overhead Tool for Floating-Point Exception Detection in NVIDIA GPUs,” in Proceedings of the 32nd International Symposium on High-Performance Parallel and Distributed Computing (HPDC ’23), pp. 59–71, 2023. doi: 10.1145/3588195.3592991 (accessed 2026-02-11).

[5] AMD, “AMD ROCm documentation,” ROCm™ Software 7.2.0. URL: [[link](https://rocm.docs.amd.com/en/latest/index.html)] (accessed 2026-02-13).

[6] AMD, “HIP documentation,” HIP 7.2.53210 Documentation. URL: [[link](https://rocm.docs.amd.com/projects/HIP/en/latest/index.html)] (accessed 2026-02-13).

[7] AMD, “OCML User Guide,” ROCm llvm-project repository, file amd/device-libs/doc/OCML.md, commit 244fe273dbe4f50cb2916e124e6f58d544a6219b. URL: [[link](https://github.com/ROCm/llvm-project/blob/244fe273dbe4f50cb2916e124e6f58d544a6219b/amd/device-libs/doc/OCML.md)] (accessed 2026-02-13).

[8] D. Miao, I. Laguna, and C. Rubio-González, “FloatGuard: Efficient Whole-Program Detection of Floating-Point Exceptions in AMD GPUs,” in Proceedings of the 34th International Symposium on High-Performance Parallel and Distributed Computing (HPDC ’25), Article No. 2, pp. 1–12, 2025. doi: 10.1145/3731545.3731586 (accessed 2026-02-13).

[9] GNU C Library Project, “libmvec - glibc wiki.” URL: [[link](https://sourceware.org/glibc/wiki/libmvec)] (accessed 2026-03-08).

[10] GNU C Library Project, “The GNU C Library version 2.22 is now available,” libc-announce mailing list, 2015-08-14. URL: [[link](https://inbox.sourceware.org/libc-announce/55CE4C0A.6030405%40redhat.com/t/)] (accessed 2026-03-09).

[11] GNU C Library Project, “The GNU C Library version 2.39 is now available,” libc-announce mailing list, 2024-01-31. URL: [[link](https://sourceware.org/pipermail/libc-announce/2024/000038.html)] (accessed 2026-03-09).

[12] Intel, “Intel® oneAPI DPC++/C++ Compiler Developer Guide and Reference,” version 2025.2. URL: [[link](https://www.intel.com/content/www/us/en/docs/dpcpp-cpp-compiler/developer-guide-reference/2025-2/overview.html)] (accessed 2026-02-20).

[13] Intel, “Intel® oneAPI Math Kernel Library Vector Mathematics Performance and Accuracy Data,” ID 772989, Version 2021.1, Date 2020-12-04. URL: [[link](https://www.intel.com/content/www/us/en/docs/onemkl/developer-reference-vector-math-performance-accuracy-data/2021-1/overview.html)] (accessed 2026-02-20).

[14] Intel, “Technology Guide | Intel® AVX-512 - FP16 Instruction Set for Intel® Xeon® Processor-Based Products,” Date 2024-03-12 (Intel content ID 669773). URL: [[link](https://builders.intel.com/docs/networkbuilders/intel-avx-512-fp16-instruction-set-for-intel-xeon-processor-based-products-technology-guide-1710148129.pdf)] (accessed 2026-02-20).

[15] N. Shibata, “SLEEF,” GitHub repository. URL: [[link](https://github.com/shibatch/sleef)] (accessed 2026-03-03).

[16] SLEEF Project, “SLEEF official documentation.” URL: [[link](https://sleef.org/)] (accessed 2026-03-03).

[17] N. Shibata and F. Petrogalli, “SLEEF: A Portable Vectorized Library of C Standard Mathematical Functions,” IEEE Transactions on Parallel and Distributed Systems, vol. 31, no. 6, pp. 1316–1327, 2020. doi: 10.1109/TPDS.2019.2960333 (accessed 2026-03-02).
