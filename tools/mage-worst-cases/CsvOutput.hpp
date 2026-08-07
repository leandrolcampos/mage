//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declares parallel CSV output for the worst-case search.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_TOOLS_MAGEWORSTCASES_CSVOUTPUT_HPP
#define MAGE_TOOLS_MAGEWORSTCASES_CSVOUTPUT_HPP

#include "llvm/Support/Error.h"

#include <memory>
#include <stddef.h>
#include <string>
#include <vector>

namespace llvm {
class raw_fd_ostream;
class raw_fd_stream;
class StringRef;
} // namespace llvm

namespace mage {
namespace worst_cases {

/// Collects concurrent CSV writes in separate temporary files.
class [[nodiscard]] CsvOutput {
public:
  static llvm::Expected<std::unique_ptr<CsvOutput>>
  create(llvm::StringRef OutputPath, size_t NumTemporaryFiles);

  ~CsvOutput() noexcept;

  CsvOutput(const CsvOutput &) = delete;
  CsvOutput &operator=(const CsvOutput &) = delete;

  void write(size_t TemporaryFileIndex, float Input, float Distance);

  llvm::Error finalize();

private:
  explicit CsvOutput(llvm::StringRef OutputPath);

  void discardOutputFile() noexcept;
  void discardTemporaryFiles() noexcept;

  std::string OutputPath;
  std::unique_ptr<llvm::raw_fd_ostream> OutputFile;
  std::vector<std::string> TemporaryPaths;
  std::vector<std::unique_ptr<llvm::raw_fd_stream>> TemporaryFiles;
};

} // namespace worst_cases
} // namespace mage

#endif // MAGE_TOOLS_MAGEWORSTCASES_CSVOUTPUT_HPP
