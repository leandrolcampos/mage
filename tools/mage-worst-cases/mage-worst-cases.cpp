//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Searches exhaustive binary32 inputs near rounding breakpoints.
///
//===----------------------------------------------------------------------===//

#include "mage/MathExtras/Bit.hpp"
#include "mage/Support/Range.hpp"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include <mpfr.h>

#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <system_error>

namespace {

constexpr unsigned Binary32Precision = 24;
constexpr mpfr_prec_t DefaultPrecision = 128;

enum class MathFunction {
  Exp,
  Log,
  TestDummy,
};

enum class RoundingKind {
  Nearest,
  Directed,
};

enum class RunMode {
  Count,
  Csv,
};

enum class OutputFormat {
  Hex,
  Bits,
};

struct InputDomain {
  float Begin;
  float End;
};

struct FunctionConfig {
  llvm::StringRef Name;
  MathFunction Kind;
  InputDomain Domain;
};

struct SearchConfig {
  const FunctionConfig *Function;
  RoundingKind Rounding;
  RunMode Run;
  OutputFormat Format;
  mpfr_prec_t Precision;
  llvm::StringRef ErrorBoundText;
  llvm::StringRef OutputDir;
};

class MpfrNumber {
public:
  explicit MpfrNumber(mpfr_prec_t Precision) { mpfr_init2(Value, Precision); }

  MpfrNumber(const MpfrNumber &) = delete;
  MpfrNumber &operator=(const MpfrNumber &) = delete;

  ~MpfrNumber() { mpfr_clear(Value); }

  operator mpfr_ptr() { return Value; }
  operator mpfr_srcptr() const { return Value; }

  [[nodiscard]] mpfr_prec_t getPrecision() const {
    return mpfr_get_prec(Value);
  }

  [[nodiscard]] int sign() const { return mpfr_sgn(Value); }

  std::error_code set(llvm::StringRef Text) {
    const std::string Storage = Text.str();
    if (mpfr_set_str(Value, Storage.c_str(), 0, MPFR_RNDN) != 0)
      return llvm::errc::invalid_argument;
    return std::error_code();
  }

  void set(float Input) {
    const int Status = mpfr_set_flt(Value, Input, MPFR_RNDN);
    (void)Status;
    assert(Status == 0 && "binary32 input must be exactly representable");
  }

  [[nodiscard]] float getRounded(mpfr_rnd_t Rounding) const {
    return mpfr_get_flt(Value, Rounding);
  }

private:
  mpfr_t Value;
};

} // namespace

using MpfrUnaryFunction = int (*)(mpfr_t, const mpfr_t, mpfr_rnd_t);

static int computeTestDummy(mpfr_t Output, const mpfr_t Input,
                            mpfr_rnd_t Rounding) {
  return mpfr_set(Output, Input, Rounding);
}

static llvm::StringRef getName(RoundingKind Rounding) {
  switch (Rounding) {
  case RoundingKind::Nearest:
    return "nearest";
  case RoundingKind::Directed:
    return "directed";
  }
  llvm_unreachable("unknown rounding kind");
}

static llvm::StringRef getName(OutputFormat Format) {
  switch (Format) {
  case OutputFormat::Hex:
    return "hex";
  case OutputFormat::Bits:
    return "bits";
  }
  llvm_unreachable("unknown output format");
}

static const FunctionConfig *getFunctionConfig(MathFunction Kind) {
  static const FunctionConfig ExpConfig = {
      "expf",
      MathFunction::Exp,
      {-std::numeric_limits<float>::denorm_min(),
       std::numeric_limits<float>::denorm_min()}};
  static constexpr FunctionConfig LogConfig = {
      "logf",
      MathFunction::Log,
      {std::numeric_limits<float>::denorm_min(),
       std::numeric_limits<float>::max()}};
  static constexpr FunctionConfig TestDummyConfig = {
      "test-dummyf",
      MathFunction::TestDummy,
      {-std::numeric_limits<float>::denorm_min(),
       std::numeric_limits<float>::denorm_min()}};

  switch (Kind) {
  case MathFunction::Exp:
    return &ExpConfig;
  case MathFunction::Log:
    return &LogConfig;
  case MathFunction::TestDummy:
    return &TestDummyConfig;
  }
  llvm_unreachable("unknown function kind");
}

static const FunctionConfig *getFunctionConfig(llvm::StringRef Name) {
  if (Name == "expf")
    return getFunctionConfig(MathFunction::Exp);
  if (Name == "logf")
    return getFunctionConfig(MathFunction::Log);
  if (Name == "test-dummyf")
    return getFunctionConfig(MathFunction::TestDummy);
  return nullptr;
}

static MpfrUnaryFunction getMpfrFunction(MathFunction Kind) {
  switch (Kind) {
  case MathFunction::Exp:
    return mpfr_exp;
  case MathFunction::Log:
    return mpfr_log;
  case MathFunction::TestDummy:
    return computeTestDummy;
  }
  llvm_unreachable("unknown function kind");
}

static void computeOutput(MpfrNumber &Output, const MpfrNumber &Input,
                          const FunctionConfig &Function) {
  MpfrUnaryFunction FunctionPtr = getMpfrFunction(Function.Kind);
  FunctionPtr(Output, Input, MPFR_RNDN);
}

static void computeUlp(MpfrNumber &Ulp, const MpfrNumber &Expected) {
  if (mpfr_zero_p(Expected) != 0) {
    mpfr_set_ui_2exp(Ulp, 1, -149, MPFR_RNDN);
    return;
  }

  const mpfr_exp_t Exponent = mpfr_get_exp(Expected);
  const mpfr_exp_t UlpExponent =
      std::max<mpfr_exp_t>(Exponent - Binary32Precision, -149);
  mpfr_set_ui_2exp(Ulp, 1, UlpExponent, MPFR_RNDN);
}

static void computeUlpError(MpfrNumber &Error, const MpfrNumber &Actual,
                            const MpfrNumber &Expected) {
  if (mpfr_number_p(Actual) == 0 || mpfr_number_p(Expected) == 0) {
    mpfr_set_inf(Error, 1);
    return;
  }

  MpfrNumber Difference(Error.getPrecision());
  MpfrNumber Ulp(Error.getPrecision());
  mpfr_sub(Difference, Expected, Actual, MPFR_RNDA);
  mpfr_abs(Difference, Difference, MPFR_RNDA);
  computeUlp(Ulp, Expected);
  mpfr_div(Error, Difference, Ulp, MPFR_RNDA);
}

static void setUnboundedBinary32Rounded(MpfrNumber &Output,
                                        const MpfrNumber &Input,
                                        mpfr_rnd_t Rounding) {
  MpfrNumber Rounded(Binary32Precision);
  mpfr_set(Rounded, Input, Rounding);
  mpfr_set(Output, Rounded, MPFR_RNDN);
}

static bool computeFloatBounds(MpfrNumber &Lower, MpfrNumber &Upper,
                               const MpfrNumber &Expected) {
  const float LowerFloat = Expected.getRounded(MPFR_RNDD);
  const float UpperFloat = Expected.getRounded(MPFR_RNDU);
  const bool HasLowerFloat = std::isfinite(LowerFloat);
  const bool HasUpperFloat = std::isfinite(UpperFloat);

  if (!HasLowerFloat && !HasUpperFloat)
    return false;

  if (!HasLowerFloat || !HasUpperFloat) {
    setUnboundedBinary32Rounded(Lower, Expected, MPFR_RNDD);
    setUnboundedBinary32Rounded(Upper, Expected, MPFR_RNDU);
    return true;
  }

  Lower.set(LowerFloat);
  Upper.set(UpperFloat);
  return true;
}

static bool computeDirectedError(MpfrNumber &Error,
                                 const MpfrNumber &Expected) {
  MpfrNumber Lower(Error.getPrecision());
  MpfrNumber Upper(Error.getPrecision());

  if (!computeFloatBounds(Lower, Upper, Expected))
    return false;

  if (mpfr_equal_p(Lower, Upper) != 0) {
    mpfr_set_ui(Error, 0, MPFR_RNDN);
    return true;
  }

  MpfrNumber CandidateError(Error.getPrecision());

  computeUlpError(Error, Lower, Expected);
  computeUlpError(CandidateError, Upper, Expected);
  if (mpfr_less_p(CandidateError, Error) != 0)
    mpfr_set(Error, CandidateError, MPFR_RNDN);

  return true;
}

static bool computeNearestError(MpfrNumber &Error, const MpfrNumber &Expected) {
  MpfrNumber Lower(Error.getPrecision());
  MpfrNumber Upper(Error.getPrecision());
  MpfrNumber Breakpoint(Error.getPrecision());

  if (!computeFloatBounds(Lower, Upper, Expected))
    return false;

  if (mpfr_equal_p(Lower, Upper) != 0) {
    mpfr_set_ui_2exp(Error, 1, -1, MPFR_RNDN);
    return true;
  }

  mpfr_sub(Breakpoint, Upper, Lower, MPFR_RNDN);
  mpfr_div_2ui(Breakpoint, Breakpoint, 1, MPFR_RNDN);
  mpfr_add(Breakpoint, Lower, Breakpoint, MPFR_RNDN);

  computeUlpError(Error, Breakpoint, Expected);
  return true;
}

static bool computeBreakpointError(MpfrNumber &Error,
                                   const MpfrNumber &Expected,
                                   RoundingKind Rounding) {
  if (mpfr_number_p(Expected) == 0)
    return false;

  if (Rounding == RoundingKind::Directed)
    return computeDirectedError(Error, Expected);

  return computeNearestError(Error, Expected);
}

static std::string getOutputPath(const SearchConfig &Config) {
  llvm::SmallString<256> Path(Config.OutputDir);
  llvm::sys::path::append(Path, (Config.Function->Name + "-" +
                                 getName(Config.Rounding) + "-" +
                                 getName(Config.Format) + ".csv")
                                    .str());
  return std::string(Path.str());
}

static void writeCsvHeader(llvm::raw_ostream &Out, OutputFormat Format) {
  switch (Format) {
  case OutputFormat::Hex:
    Out << "input,error\n";
    break;
  case OutputFormat::Bits:
    Out << "input_bits,error_bits\n";
    break;
  }
}

static void writeHexFloat(llvm::raw_ostream &Out, float Value) {
  Out << llvm::format("%a", static_cast<double>(Value));
}

static void writeCsvRow(llvm::raw_ostream &Out, float Input, float Error,
                        OutputFormat Format) {
  switch (Format) {
  case OutputFormat::Hex:
    writeHexFloat(Out, Input);
    Out << ',';
    writeHexFloat(Out, Error);
    Out << '\n';
    break;
  case OutputFormat::Bits:
    Out << llvm::format_hex(mage::bit_cast<uint32_t>(Input), 10);
    Out << ',';
    Out << llvm::format_hex(mage::bit_cast<uint32_t>(Error), 10);
    Out << '\n';
    break;
  }
}

static bool searchWorstCases(const SearchConfig &Config) {
  if (Config.Precision < Binary32Precision) {
    llvm::WithColor::error()
        << "precision must be at least " << Binary32Precision << " bits\n";
    return false;
  }

  MpfrNumber Input(Config.Precision);
  MpfrNumber Output(Config.Precision);
  MpfrNumber Error(Config.Precision);
  MpfrNumber ErrorBoundValue(Config.Precision);

  if (ErrorBoundValue.set(Config.ErrorBoundText)) {
    llvm::WithColor::error()
        << "invalid error bound: " << Config.ErrorBoundText << '\n';
    return false;
  }

  if (mpfr_number_p(ErrorBoundValue) == 0) {
    llvm::WithColor::error() << "error bound must be finite\n";
    return false;
  }

  if (ErrorBoundValue.sign() < 0) {
    llvm::WithColor::error() << "error bound must be non-negative\n";
    return false;
  }

  std::string OutputPath;
  std::unique_ptr<llvm::raw_fd_ostream> OutputFile;

  if (Config.Run == RunMode::Csv) {
    OutputPath = getOutputPath(Config);
    std::error_code EC;
    OutputFile = std::make_unique<llvm::raw_fd_ostream>(OutputPath, EC,
                                                        llvm::sys::fs::OF_Text);
    if (EC) {
      llvm::WithColor::error() << OutputPath << ": " << EC.message() << '\n';
      return false;
    }
    writeCsvHeader(*OutputFile, Config.Format);
  }

  uint64_t WorstCaseCount = 0;
  const mage::range<float> Inputs(Config.Function->Domain.Begin,
                                  Config.Function->Domain.End);

  for (float Value : Inputs) {
    Input.set(Value);
    computeOutput(Output, Input, *Config.Function);

    if (!computeBreakpointError(Error, Output, Config.Rounding))
      continue;

    if (mpfr_less_p(Error, ErrorBoundValue) == 0)
      continue;

    ++WorstCaseCount;

    if (Config.Run == RunMode::Csv) {
      const float ErrorAsFloat = Error.getRounded(MPFR_RNDN);
      writeCsvRow(*OutputFile, Value, ErrorAsFloat, Config.Format);
    }
  }

  if (Config.Run == RunMode::Count)
    llvm::outs() << "Worst case count: " << WorstCaseCount << '\n';
  else
    llvm::outs() << "Wrote " << WorstCaseCount << " worst case(s) to "
                 << OutputPath << '\n';

  return true;
}

int main(int Argc, char **Argv) {
  llvm::InitLLVM InitLLVM(Argc, Argv);

  llvm::cl::OptionCategory WorstCasesCategory("mage-worst-cases options");

  llvm::cl::opt<std::string> FunctionName(
      llvm::cl::Positional, llvm::cl::desc("<function: expf|logf|test-dummyf>"),
      llvm::cl::ValueRequired, llvm::cl::Required,
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<RoundingKind> Rounding(
      "rounding", llvm::cl::desc("rounding breakpoint kind to test"),
      llvm::cl::values(
          clEnumValN(RoundingKind::Nearest, "nearest",
                     "test round-to-nearest midpoints"),
          clEnumValN(RoundingKind::Directed, "directed",
                     "test directed-rounding floating-point breakpoints")),
      llvm::cl::init(RoundingKind::Nearest), llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<RunMode> Run(
      "run-mode", llvm::cl::desc("search output mode"),
      llvm::cl::values(clEnumValN(RunMode::Count, "count",
                                  "print only the number of inputs found"),
                       clEnumValN(RunMode::Csv, "csv",
                                  "write the inputs found to a CSV file")),
      llvm::cl::init(RunMode::Csv), llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<OutputFormat> Format(
      "output-format", llvm::cl::desc("CSV numeric representation"),
      llvm::cl::values(clEnumValN(OutputFormat::Hex, "hex",
                                  "write hexadecimal floating-point literals"),
                       clEnumValN(OutputFormat::Bits, "bits",
                                  "write uint32 bit patterns in hexadecimal")),
      llvm::cl::init(OutputFormat::Hex), llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<std::string> OutputDir(
      "output-dir", llvm::cl::desc("directory where CSV output is written"),
      llvm::cl::value_desc("path"), llvm::cl::init("."),
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<std::string> ErrorBound(
      "error-bound", llvm::cl::desc("maximum ULP distance from a breakpoint"),
      llvm::cl::value_desc("number"), llvm::cl::init("0x1p-40"),
      llvm::cl::cat(WorstCasesCategory));

  llvm::cl::opt<unsigned> Precision(
      "precision", llvm::cl::desc("MPFR precision in bits"),
      llvm::cl::init(DefaultPrecision), llvm::cl::cat(WorstCasesCategory));

  llvm::cl::HideUnrelatedOptions(WorstCasesCategory);
  llvm::cl::ParseCommandLineOptions(
      Argc, Argv, "Search for binary32 worst cases in elementary functions\n");

  const FunctionConfig *Function = getFunctionConfig(FunctionName);
  if (Function == nullptr) {
    llvm::WithColor::error() << "unknown function '" << FunctionName
                             << "'; expected expf, logf, or test-dummyf\n";
    return 1;
  }

  const SearchConfig Config = {Function,  Rounding,   Run,      Format,
                               Precision, ErrorBound, OutputDir};

  return searchWorstCases(Config) ? 0 : 1;
}
