//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file declares logging support for Mage unit tests.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_UNITTESTS_UNITTEST_TESTLOGGER_HPP
#define MAGE_UNITTESTS_UNITTEST_TESTLOGGER_HPP

namespace mage {
namespace testing {

// A class to log to standard error in the context of unit tests.
struct TestLogger {
  constexpr TestLogger() = default;

  TestLogger &operator<<(const char *);
  TestLogger &operator<<(decltype(nullptr));
  TestLogger &operator<<(char);
  TestLogger &operator<<(bool);

  TestLogger &operator<<(short);
  TestLogger &operator<<(unsigned short);
  TestLogger &operator<<(int);
  TestLogger &operator<<(unsigned);
  TestLogger &operator<<(long);
  TestLogger &operator<<(unsigned long);
  TestLogger &operator<<(long long);
  TestLogger &operator<<(unsigned long long);

  TestLogger &operator<<(const void *);

  TestLogger &operator<<(_Float16);
  TestLogger &operator<<(float);
  TestLogger &operator<<(double);
};

// Returns the global test logger to be used in unit tests.
TestLogger &tlog();

} // namespace testing
} // namespace mage

#endif // MAGE_UNITTESTS_UNITTEST_TESTLOGGER_HPP
