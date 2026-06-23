//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares access to the HIP backend.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_LIB_OFFLOAD_HIP_HIPBACKEND_HPP
#define MAGE_LIB_OFFLOAD_HIP_HIPBACKEND_HPP

#include "Backend.hpp"

namespace mage {
namespace detail {

[[nodiscard]] llvm::Expected<Backend &> getHIPBackend();

} // namespace detail
} // namespace mage

#endif // MAGE_LIB_OFFLOAD_HIP_HIPBACKEND_HPP
