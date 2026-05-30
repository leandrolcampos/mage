//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file declares the core Mage unit-test framework.
///
/// Some of the code in this file is adapted from:
///
/// llvm/llvm-project:
/// Copyright the LLVM Project contributors.
/// Licensed under Apache License v2.0 with LLVM Exceptions.
///
//===----------------------------------------------------------------------===//

#ifndef MAGE_UNITTESTS_UNITTEST_TEST_HPP
#define MAGE_UNITTESTS_UNITTEST_TEST_HPP

#include "UnitTest/TestLogger.hpp"

#include <stddef.h>
#include <string.h>

namespace mage {
namespace testing {

enum class TestCond { EQ, NE, LT, LE, GT, GE };

namespace internal {

struct Location {
  constexpr Location(const char *File, int Line) : File(File), Line(Line) {}

  const char *File;
  int Line;
};

class RunContext {
public:
  enum class RunResult : bool { Pass, Fail };

  RunResult status() const { return Status; }
  bool hasFatalFailure() const { return HasFatalFailure; }

  void markFail() { Status = RunResult::Fail; }
  void markFatalFail() {
    markFail();
    HasFatalFailure = true;
  }

private:
  RunResult Status = RunResult::Pass;
  bool HasFatalFailure = false;
};

TestLogger &operator<<(TestLogger &Logger, Location Loc);

template <TestCond Cond> constexpr const char *getConditionString() {
  if constexpr (Cond == TestCond::EQ)
    return "equal to";
  if constexpr (Cond == TestCond::NE)
    return "not equal to";
  if constexpr (Cond == TestCond::LT)
    return "less than";
  if constexpr (Cond == TestCond::LE)
    return "less than or equal to";
  if constexpr (Cond == TestCond::GT)
    return "greater than";
  if constexpr (Cond == TestCond::GE)
    return "greater than or equal to";

  __builtin_unreachable();
}

constexpr size_t getStringLength(const char *Str) {
  if (Str == nullptr)
    return 0;

  size_t Length = 0;
  while (Str[Length] != '\0')
    ++Length;

  return Length;
}

template <TestCond Cond, typename LHSType, typename RHSType>
bool evaluate(const LHSType &LHS, const RHSType &RHS) {
  if constexpr (Cond == TestCond::EQ)
    return LHS == RHS;
  if constexpr (Cond == TestCond::NE)
    return LHS != RHS;
  if constexpr (Cond == TestCond::LT)
    return LHS < RHS;
  if constexpr (Cond == TestCond::LE)
    return LHS <= RHS;
  if constexpr (Cond == TestCond::GT)
    return LHS > RHS;
  if constexpr (Cond == TestCond::GE)
    return LHS >= RHS;

  __builtin_unreachable();
}

template <TestCond Cond>
bool evaluateCString(const char *LHS, const char *RHS) {
  const bool Equal =
      LHS == RHS || (LHS != nullptr && RHS != nullptr && strcmp(LHS, RHS) == 0);
  if constexpr (Cond == TestCond::EQ)
    return Equal;
  if constexpr (Cond == TestCond::NE)
    return !Equal;

  __builtin_unreachable();
}

template <TestCond Cond, bool IsCString = false, typename LHSType,
          typename RHSType>
bool test(RunContext *Ctx, const LHSType &LHS, const RHSType &RHS,
          const char *LHSStr, const char *RHSStr, Location Loc) {
  bool Passed;
  if constexpr (IsCString)
    Passed = evaluateCString<Cond>(LHS, RHS);
  else
    Passed = evaluate<Cond>(LHS, RHS);

  if (Passed)
    return true;

  Ctx->markFail();

  constexpr const char *CondString = getConditionString<Cond>();
  constexpr size_t CondStringLength = getStringLength(CondString);
  constexpr size_t OffsetLength =
      CondStringLength > 2 ? CondStringLength - 2 : 0;
  char Offset[OffsetLength + 1];
  memset(Offset, ' ', OffsetLength);
  Offset[OffsetLength] = '\0';

  tlog() << Loc;
  tlog() << Offset << "Expected: " << LHSStr << '\n';
  tlog() << Offset << "Which is: " << LHS << '\n';
  tlog() << "To be " << CondString << ": " << RHSStr << '\n';
  tlog() << Offset << "Which is: " << RHS << '\n';
  return false;
}

} // namespace internal

class Test {
public:
  virtual ~Test();
  virtual void setUp() {}
  virtual void tearDown() {}

  static int runTests();

protected:
  constexpr Test() = default;

  template <TestCond Cond, bool IsCString = false, typename LHSType,
            typename RHSType>
  bool test(const LHSType &LHS, const RHSType &RHS, const char *LHSStr,
            const char *RHSStr, internal::Location Loc) {
    return internal::test<Cond, IsCString>(Ctx, LHS, RHS, LHSStr, RHSStr, Loc);
  }

  static void addTest(Test *T);

private:
  void setContext(internal::RunContext *C) { Ctx = C; }

  virtual const char *getName() const = 0;
  virtual void run() = 0;

  internal::RunContext *Ctx = nullptr;
  Test *Next = nullptr;

  static int getNumTests();

  static Test *Start;
  static Test *End;
};

} // namespace testing
} // namespace mage

#define MAGE_TEST_LOC_() ::mage::testing::internal::Location(__FILE__, __LINE__)

#define MAGE_TEST(SuiteName, TestName)                                         \
  namespace {                                                                  \
  class SuiteName##_##TestName : public mage::testing::Test {                  \
  public:                                                                      \
    SuiteName##_##TestName() { addTest(this); }                                \
                                                                               \
  private:                                                                     \
    void run() override;                                                       \
    const char *getName() const override { return #SuiteName "." #TestName; }  \
  };                                                                           \
                                                                               \
  SuiteName##_##TestName SuiteName##_##TestName##_Instance;                    \
  }                                                                            \
  void SuiteName##_##TestName::run()

#define MAGE_TEST_F(SuiteClass, TestName)                                      \
  namespace {                                                                  \
  class SuiteClass##_##TestName : public SuiteClass {                          \
  public:                                                                      \
    SuiteClass##_##TestName() { addTest(this); }                               \
                                                                               \
  private:                                                                     \
    void run() override;                                                       \
    const char *getName() const override { return #SuiteClass "." #TestName; } \
  };                                                                           \
                                                                               \
  SuiteClass##_##TestName SuiteClass##_##TestName##_Instance;                  \
  }                                                                            \
  void SuiteClass##_##TestName::run()

//===----------------------------------------------------------------------===//
// Binary comparison checks
//===----------------------------------------------------------------------===//

#define MAGE_TEST_BINOP_(Cond, LHS, RHS, OnFailure)                            \
  do {                                                                         \
    if (!this->test<::mage::testing::TestCond::Cond>((LHS), (RHS), #LHS, #RHS, \
                                                     MAGE_TEST_LOC_())) {      \
      OnFailure;                                                               \
    }                                                                          \
  } while (false)

#define MAGE_EXPECT_EQ(LHS, RHS) MAGE_TEST_BINOP_(EQ, LHS, RHS, )
#define MAGE_ASSERT_EQ(LHS, RHS) MAGE_TEST_BINOP_(EQ, LHS, RHS, return)

#define MAGE_EXPECT_NE(LHS, RHS) MAGE_TEST_BINOP_(NE, LHS, RHS, )
#define MAGE_ASSERT_NE(LHS, RHS) MAGE_TEST_BINOP_(NE, LHS, RHS, return)

#define MAGE_EXPECT_LT(LHS, RHS) MAGE_TEST_BINOP_(LT, LHS, RHS, )
#define MAGE_ASSERT_LT(LHS, RHS) MAGE_TEST_BINOP_(LT, LHS, RHS, return)

#define MAGE_EXPECT_LE(LHS, RHS) MAGE_TEST_BINOP_(LE, LHS, RHS, )
#define MAGE_ASSERT_LE(LHS, RHS) MAGE_TEST_BINOP_(LE, LHS, RHS, return)

#define MAGE_EXPECT_GT(LHS, RHS) MAGE_TEST_BINOP_(GT, LHS, RHS, )
#define MAGE_ASSERT_GT(LHS, RHS) MAGE_TEST_BINOP_(GT, LHS, RHS, return)

#define MAGE_EXPECT_GE(LHS, RHS) MAGE_TEST_BINOP_(GE, LHS, RHS, )
#define MAGE_ASSERT_GE(LHS, RHS) MAGE_TEST_BINOP_(GE, LHS, RHS, return)

//===----------------------------------------------------------------------===//
// Boolean checks
//===----------------------------------------------------------------------===//

#define MAGE_TEST_BOOL_(Expected, Val, OnFailure)                              \
  do {                                                                         \
    if (!this->test<::mage::testing::TestCond::EQ>(static_cast<bool>(Val),     \
                                                   Expected, #Val, #Expected,  \
                                                   MAGE_TEST_LOC_())) {        \
      OnFailure;                                                               \
    }                                                                          \
  } while (false)

#define MAGE_EXPECT_TRUE(Val) MAGE_TEST_BOOL_(true, Val, )
#define MAGE_ASSERT_TRUE(Val) MAGE_TEST_BOOL_(true, Val, return)

#define MAGE_EXPECT_FALSE(Val) MAGE_TEST_BOOL_(false, Val, )
#define MAGE_ASSERT_FALSE(Val) MAGE_TEST_BOOL_(false, Val, return)

//===----------------------------------------------------------------------===//
// C string checks
//===----------------------------------------------------------------------===//

#define MAGE_TEST_CSTRING_(Cond, LHS, RHS, OnFailure)                          \
  do {                                                                         \
    if (!this->test<::mage::testing::TestCond::Cond, true>(                    \
            (LHS), (RHS), #LHS, #RHS, MAGE_TEST_LOC_())) {                     \
      OnFailure;                                                               \
    }                                                                          \
  } while (false)

#define MAGE_EXPECT_STREQ(LHS, RHS) MAGE_TEST_CSTRING_(EQ, LHS, RHS, )
#define MAGE_ASSERT_STREQ(LHS, RHS) MAGE_TEST_CSTRING_(EQ, LHS, RHS, return)

#define MAGE_EXPECT_STRNE(LHS, RHS) MAGE_TEST_CSTRING_(NE, LHS, RHS, )
#define MAGE_ASSERT_STRNE(LHS, RHS) MAGE_TEST_CSTRING_(NE, LHS, RHS, return)

#endif // MAGE_UNITTESTS_UNITTEST_TEST_HPP
