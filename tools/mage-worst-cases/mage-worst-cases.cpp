//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the entry point for the mage-worst-cases tool.
///
//===----------------------------------------------------------------------===//

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"

#include <mpfr.h>

#include <iostream>

int main(int Argc, char **Argv) {
  llvm::InitLLVM InitLLVM(Argc, Argv);

  llvm::cl::ParseCommandLineOptions(
      Argc, Argv, "Search for worst cases in Mage numerical functions\n");

  std::cout << "MPFR version: " << mpfr_get_version() << '\n';
  return 0;
}
