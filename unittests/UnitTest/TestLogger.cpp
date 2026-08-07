//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements logging for Mage unit tests.
///
//===----------------------------------------------------------------------===//

#include "UnitTest/TestLogger.hpp"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

using namespace mage;

namespace mage {
namespace testing {

TestLogger &TestLogger::operator<<(const char *Str) noexcept {
  if (Str == nullptr)
    return *this << "(null)";

  fprintf(stderr, "%s", Str);
  return *this;
}

TestLogger &TestLogger::operator<<(decltype(nullptr)) noexcept {
  return *this << "nullptr";
}

TestLogger &TestLogger::operator<<(char C) noexcept {
  fprintf(stderr, "%c", C);
  return *this;
}

TestLogger &TestLogger::operator<<(bool Cond) noexcept {
  return *this << (Cond ? "true" : "false");
}

TestLogger &TestLogger::operator<<(short N) noexcept {
  fprintf(stderr, "%hd", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned short N) noexcept {
  fprintf(stderr, "%hu", N);
  return *this;
}

TestLogger &TestLogger::operator<<(int N) noexcept {
  fprintf(stderr, "%d", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned N) noexcept {
  fprintf(stderr, "%u", N);
  return *this;
}

TestLogger &TestLogger::operator<<(long N) noexcept {
  fprintf(stderr, "%ld", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned long N) noexcept {
  fprintf(stderr, "%lu", N);
  return *this;
}

TestLogger &TestLogger::operator<<(long long N) noexcept {
  fprintf(stderr, "%lld", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned long long N) noexcept {
  fprintf(stderr, "%llu", N);
  return *this;
}

TestLogger &TestLogger::operator<<(const void *Ptr) noexcept {
  fprintf(stderr, "0x%" PRIxPTR, reinterpret_cast<uintptr_t>(Ptr));
  return *this;
}

TestLogger &TestLogger::operator<<(_Float16 X) noexcept {
  fprintf(stderr, "%.5g", static_cast<double>(X));
  return *this;
}

TestLogger &TestLogger::operator<<(float X) noexcept {
  fprintf(stderr, "%.9g", static_cast<double>(X));
  return *this;
}

TestLogger &TestLogger::operator<<(double X) noexcept {
  fprintf(stderr, "%.17g", X);
  return *this;
}

} // namespace testing
} // namespace mage

testing::TestLogger &testing::tlog() noexcept {
  static TestLogger TestLog;
  return TestLog;
}
