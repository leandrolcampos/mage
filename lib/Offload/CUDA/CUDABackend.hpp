//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares access to the CUDA backend.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_LIB_OFFLOAD_CUDA_CUDABACKEND_HPP
#define MAGE_LIB_OFFLOAD_CUDA_CUDABACKEND_HPP

#include "Backend.hpp"

namespace mage {
namespace detail {

[[nodiscard]] llvm::Expected<Backend &> getCUDABackend();

} // namespace detail
} // namespace mage

#endif // MAGE_LIB_OFFLOAD_CUDA_CUDABACKEND_HPP
