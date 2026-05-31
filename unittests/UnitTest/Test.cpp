//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implements the core Mage unit-test framework.
///
/// Some of the code in this file is adapted from:
///
/// llvm/llvm-project:
/// Copyright the LLVM Project contributors.
/// Licensed under Apache License v2.0 with LLVM Exceptions.
///
//===----------------------------------------------------------------------===//

#include "UnitTest/Test.hpp"

namespace mage {
namespace testing {

TestLogger &detail::operator<<(TestLogger &Logger, detail::Location Loc) {
  return Logger << Loc.File << ':' << Loc.Line << ": FAILURE\n";
}

Test::~Test() = default;

int Test::runTests() {
  const int TestCount = getNumTests();
  if (TestCount == 0) {
    tlog() << "No tests run.\n";
    return 1;
  }

  tlog() << "[==========] Running " << TestCount << " test";
  if (TestCount != 1)
    tlog() << 's';
  tlog() << ".\n";

  int FailCount = 0;
  for (Test *T = Start; T != nullptr; T = T->Next) {
    const char *TestName = T->getName();
    tlog() << "[ RUN      ] " << TestName << '\n';

    detail::RunContext Ctx;

    T->setContext(&Ctx);
    T->setUp();

    if (!Ctx.hasFatalFailure())
      T->run();

    T->tearDown();
    T->setContext(nullptr);

    if (Ctx.status() == detail::RunContext::RunResult::Fail) {
      ++FailCount;
      tlog() << "[  FAILED  ] " << TestName << '\n';
    } else {
      tlog() << "[       OK ] " << TestName << '\n';
    }
  }

  tlog() << "[==========] Ran " << TestCount << " test";
  if (TestCount != 1)
    tlog() << 's';
  tlog() << ".\n";

  if (FailCount == 0) {
    tlog() << "[  PASSED  ] " << TestCount << " test";
    if (TestCount != 1)
      tlog() << 's';
    tlog() << ".\n";
    return 0;
  }

  tlog() << "[  FAILED  ] " << FailCount << " test";
  if (FailCount != 1)
    tlog() << 's';
  tlog() << ".\n";
  return 1;
}

void Test::addTest(Test *T) {
  if (End == nullptr) {
    Start = T;
    End = T;
    return;
  }

  End->Next = T;
  End = T;
}

int Test::getNumTests() {
  int N = 0;
  for (Test *T = Start; T != nullptr; T = T->Next)
    ++N;
  return N;
}

Test *Test::Start = nullptr;
Test *Test::End = nullptr;

} // namespace testing
} // namespace mage
