//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements logging for Mage unit tests.
///
//===----------------------------------------------------------------------===//

#include "UnitTest/TestLogger.hpp"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

namespace mage {
namespace testing {

TestLogger &TestLogger::operator<<(const char *Str) {
  if (Str == nullptr)
    return *this << "(null)";

  fprintf(stderr, "%s", Str);
  return *this;
}

TestLogger &TestLogger::operator<<(decltype(nullptr)) {
  return *this << "nullptr";
}

TestLogger &TestLogger::operator<<(char C) {
  fprintf(stderr, "%c", C);
  return *this;
}

TestLogger &TestLogger::operator<<(bool Cond) {
  return *this << (Cond ? "true" : "false");
}

TestLogger &TestLogger::operator<<(short N) {
  fprintf(stderr, "%hd", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned short N) {
  fprintf(stderr, "%hu", N);
  return *this;
}

TestLogger &TestLogger::operator<<(int N) {
  fprintf(stderr, "%d", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned N) {
  fprintf(stderr, "%u", N);
  return *this;
}

TestLogger &TestLogger::operator<<(long N) {
  fprintf(stderr, "%ld", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned long N) {
  fprintf(stderr, "%lu", N);
  return *this;
}

TestLogger &TestLogger::operator<<(long long N) {
  fprintf(stderr, "%lld", N);
  return *this;
}

TestLogger &TestLogger::operator<<(unsigned long long N) {
  fprintf(stderr, "%llu", N);
  return *this;
}

TestLogger &TestLogger::operator<<(const void *Addr) {
  fprintf(stderr, "0x%" PRIxPTR, reinterpret_cast<uintptr_t>(Addr));
  return *this;
}

TestLogger &TestLogger::operator<<(_Float16 X) {
  fprintf(stderr, "%.5g", static_cast<double>(X));
  return *this;
}

TestLogger &TestLogger::operator<<(float X) {
  fprintf(stderr, "%.9g", static_cast<double>(X));
  return *this;
}

TestLogger &TestLogger::operator<<(double X) {
  fprintf(stderr, "%.17g", X);
  return *this;
}

TestLogger &tlog() {
  static TestLogger TestLog;
  return TestLog;
}

} // namespace testing
} // namespace mage
