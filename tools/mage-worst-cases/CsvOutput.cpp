//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements parallel CSV output for the worst-case search.
///
//===----------------------------------------------------------------------===//

#include "CsvOutput.hpp"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <array>
#include <cassert>
#include <stdint.h>
#include <system_error>
#include <utility>

using namespace mage;

static llvm::Error createFileError(llvm::StringRef Operation,
                                   llvm::StringRef Path, std::error_code EC) {
  return llvm::createStringError(EC, "failed to %s '%s'",
                                 Operation.str().c_str(), Path.str().c_str());
}

static llvm::Error getOutputError(llvm::raw_fd_ostream &Output,
                                  llvm::StringRef Operation,
                                  llvm::StringRef Path) {
  if (!Output.has_error())
    return llvm::Error::success();

  const std::error_code EC = Output.error();
  Output.clear_error();
  return createFileError(Operation, Path, EC);
}

namespace mage {
namespace worst_cases {

CsvOutput::CsvOutput(llvm::StringRef OutputPath) : OutputPath(OutputPath) {}

CsvOutput::~CsvOutput() noexcept {
  discardOutputFile();
  discardTemporaryFiles();
}

llvm::Expected<std::unique_ptr<CsvOutput>>
CsvOutput::create(llvm::StringRef OutputPath, size_t NumTemporaryFiles) {
  auto Output = std::unique_ptr<CsvOutput>(new CsvOutput(OutputPath));

  std::error_code EC;
  auto OutputFile = std::make_unique<llvm::raw_fd_ostream>(
      OutputPath, EC, llvm::sys::fs::CD_CreateNew, llvm::sys::fs::FA_Write,
      llvm::sys::fs::OF_Text);
  if (EC)
    return createFileError("create output file", OutputPath, EC);
  Output->OutputFile = std::move(OutputFile);

  Output->TemporaryPaths.reserve(NumTemporaryFiles);
  Output->TemporaryFiles.reserve(NumTemporaryFiles);

  const std::string Model = (OutputPath + ".part-%%%%%%").str();
  for (size_t TemporaryFileIndex = 0; TemporaryFileIndex < NumTemporaryFiles;
       ++TemporaryFileIndex) {
    int FileDescriptor = -1;
    llvm::SmallString<256> TemporaryPath;
    if (const std::error_code EC = llvm::sys::fs::createUniqueFile(
            Model, FileDescriptor, TemporaryPath, llvm::sys::fs::OF_Text))
      return createFileError("create temporary file for", OutputPath, EC);

    Output->TemporaryPaths.emplace_back(TemporaryPath.str());
    Output->TemporaryFiles.push_back(
        std::make_unique<llvm::raw_fd_stream>(FileDescriptor, true));
  }

  return std::move(Output);
}

void CsvOutput::write(size_t TemporaryFileIndex, float Input, float Distance) {
  assert((TemporaryFileIndex < TemporaryFiles.size()) &&
         "temporary file index must refer to a temporary file");

  llvm::raw_ostream &Out = *TemporaryFiles[TemporaryFileIndex];
  Out << llvm::format("%a,%a\n", static_cast<double>(Input),
                      static_cast<double>(Distance));
}

llvm::Error CsvOutput::finalize() {
  constexpr size_t CopyBufferSize = 64 * 1024;

  *OutputFile << "input,distance\n";
  std::array<char, CopyBufferSize> Buffer;

  for (size_t TemporaryFileIndex = 0;
       TemporaryFileIndex < TemporaryFiles.size(); ++TemporaryFileIndex) {
    llvm::raw_fd_stream &TemporaryFile = *TemporaryFiles[TemporaryFileIndex];
    TemporaryFile.seek(0);
    if (llvm::Error Error =
            getOutputError(TemporaryFile, "prepare temporary file",
                           TemporaryPaths[TemporaryFileIndex])) {
      discardOutputFile();
      return Error;
    }

    while (true) {
      const ssize_t ReadSize = TemporaryFile.read(Buffer.data(), Buffer.size());
      if (ReadSize < 0) {
        llvm::Error Error = getOutputError(TemporaryFile, "read temporary file",
                                           TemporaryPaths[TemporaryFileIndex]);
        discardOutputFile();
        return Error;
      }
      if (ReadSize == 0)
        break;
      OutputFile->write(Buffer.data(), static_cast<size_t>(ReadSize));
    }
  }

  OutputFile->close();
  if (llvm::Error Error =
          getOutputError(*OutputFile, "write output file", OutputPath)) {
    (void)llvm::sys::fs::remove(OutputPath);
    OutputFile.reset();
    return Error;
  }
  OutputFile.reset();

  for (size_t TemporaryFileIndex = 0;
       TemporaryFileIndex < TemporaryFiles.size(); ++TemporaryFileIndex) {
    llvm::raw_fd_stream &TemporaryFile = *TemporaryFiles[TemporaryFileIndex];
    TemporaryFile.close();
    if (llvm::Error Error =
            getOutputError(TemporaryFile, "close temporary file",
                           TemporaryPaths[TemporaryFileIndex])) {
      TemporaryFiles[TemporaryFileIndex].reset();
      return Error;
    }
    TemporaryFiles[TemporaryFileIndex].reset();

    if (const std::error_code RemoveEC =
            llvm::sys::fs::remove(TemporaryPaths[TemporaryFileIndex]))
      return createFileError("remove temporary file",
                             TemporaryPaths[TemporaryFileIndex], RemoveEC);
    TemporaryPaths[TemporaryFileIndex].clear();
  }

  TemporaryFiles.clear();
  TemporaryPaths.clear();
  return llvm::Error::success();
}

void CsvOutput::discardOutputFile() noexcept {
  if (OutputFile == nullptr)
    return;

  OutputFile->close();
  OutputFile->clear_error();
  OutputFile.reset();
  (void)llvm::sys::fs::remove(OutputPath);
}

void CsvOutput::discardTemporaryFiles() noexcept {
  for (size_t TemporaryFileIndex = 0;
       TemporaryFileIndex < TemporaryFiles.size(); ++TemporaryFileIndex) {
    if (TemporaryPaths[TemporaryFileIndex].empty())
      continue;

    if (TemporaryFiles[TemporaryFileIndex] != nullptr) {
      TemporaryFiles[TemporaryFileIndex]->close();
      TemporaryFiles[TemporaryFileIndex]->clear_error();
    }
    (void)llvm::sys::fs::remove(TemporaryPaths[TemporaryFileIndex]);
  }
}

} // namespace worst_cases
} // namespace mage
