/* SPDX-License-Identifier: LGPL-3.0-only */
#include <fstream>
#include <iomanip>
#include <memory>
#include <utility>

#include "monomux/adt/Ranges.hpp"
#include "monomux/system/UnixPipe.hpp"
#include "monomux/system/UnixProcess.hpp"
#include "monomux/system/fd.hpp"

#include "UnixBacktraceDetail.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Backtrace")

namespace
{

constexpr std::size_t BinaryPipeCommunicationSize = 4096;

} // namespace

namespace monomux::system::unix
{

void Symboliser::noSymbolisersMessage()
{
  MONOMUX_TRACE_LOG(
    LOG(warn) << "No symboliser found. Please install 'binutils' for addr2line "
                 "or 'llvm-symbolizer', or ensure they are in the PATH!");
}

bool Symboliser::check() const
{
  using namespace monomux::system::unix;

  Process::SpawnOptions SO;
  SO.Program = Binary;
  SO.Arguments.emplace_back("--help");

  Pipe::AnonymousPipe Pipe = Pipe::create(true);
  Pipe::AnonymousPipe PipeErr = Pipe::create(true);
  static_cast<class Pipe*>(Pipe.getRead())->setNonblocking();
  SO.StandardInput.emplace(fd::Traits::Invalid);
  SO.StandardOutput.emplace(Pipe.getWrite()->raw());
  SO.StandardError.emplace(PipeErr.getWrite()->raw());

  std::unique_ptr<system::Process> P = Process::spawn(SO);
  P->wait();

  std::string Out = Pipe.getRead()->read(BinaryPipeCommunicationSize);
  return !Out.empty();
}

namespace
{

/// \p Symboliser implementation for GNU \p addr2line.
struct Addr2Line : public Symboliser
{
  Addr2Line() : Symboliser("addr2line") {}

  std::vector<std::optional<Backtrace::Symbol>>
  symbolise(const std::string& Object,
            const std::vector<Backtrace::Frame*>& Frames) const override;
};

/// \p Symboliser implementation for LLVM's symboliser.
struct LLVMSymbolizer : public Symboliser
{
  LLVMSymbolizer() : Symboliser("llvm-symbolizer") {}

  std::vector<std::optional<Backtrace::Symbol>>
  symbolise(const std::string& Object,
            const std::vector<Backtrace::Frame*>& Frames) const override;
};

std::vector<std::optional<Backtrace::Symbol>>
Addr2Line::symbolise(const std::string& Object,
                     const std::vector<Backtrace::Frame*>& Frames) const
{
  using namespace monomux::system::unix;

  std::vector<std::optional<Backtrace::Symbol>> Result;
  Result.reserve(Frames.size());

  Process::SpawnOptions SO;
  SO.Program = name();
  SO.Arguments.emplace_back("--exe"); // Specify binary to read.
  SO.Arguments.emplace_back(Object);
  SO.Arguments.emplace_back("--addresses");
  SO.Arguments.emplace_back("--functions");
  SO.Arguments.emplace_back("--demangle");
  SO.Arguments.emplace_back("--inlines");
  const std::size_t AddressesBeginAt = SO.Arguments.size();

  for (Backtrace::Frame* F : Frames)
  {
    std::ostringstream OS;

    if (F->ImageOffset)
      OS << F->ImageOffset;
    else if (!F->Data.Offset.empty())
      OS << F->Data.Offset;
    else if (!F->Data.HexAddress.empty())
      OS << F->Data.HexAddress;
    else
      OS << "0x0";

    // Strip "0x"
    SO.Arguments.emplace_back(OS.str().substr(2));
  }

  Pipe::AnonymousPipe OutPipe = Pipe::create(true);
  Pipe::AnonymousPipe ErrPipe = Pipe::create(true);
  static_cast<class Pipe*>(OutPipe.getRead())->setNonblocking();
  SO.StandardInput.emplace(fd::Traits::Invalid);
  SO.StandardOutput.emplace(OutPipe.getWrite()->raw());
  SO.StandardError.emplace(ErrPipe.getWrite()->raw());

  std::unique_ptr<system::Process> P = Process::spawn(SO);
  P->wait();

  std::string Output;
  std::size_t CurrentFrame = 0;
  do
  {
    Output.append(OutPipe.getRead()->read(BinaryPipeCommunicationSize));

    // The output format is:
    //
    //    address for symbol 1
    //    name for symbol 1
    //    location for symbol 1
    //    inlining symbol A for symbol 1        <- Optional.
    //    location for inlining symbol A        <- Optional.
    //    ... more inlining locations ...       <- Optional.
    //    address for symbol 2
    //    name for symbol 2
    //    location for symbol 2
    //    ...

    auto Pos = std::string::npos;
    do
    {
      // The lines in the symboliser output corresponding to the current frame.
      std::vector<std::string_view> Lines;

      // Find a block of lines separated by a new address or newline after a
      // newline. This corresponds to the information about one stack frame
      // address.
      Pos = Output.find("\n0x");
      if (Pos == std::string::npos)
      {
        Pos = Output.find_last_of('\n');
        if (Pos == std::string::npos)
          break;
      }

      std::string::size_type LastPosition = 0;
      std::string::size_type PosInBlock = Output.find('\n', 1);
      std::string_view Address =
        ranges::stringRange(Output.begin(), Output.begin() + PosInBlock);
      while (PosInBlock <= Pos)
      {
        LastPosition = PosInBlock;
        PosInBlock = Output.find('\n', LastPosition + 1);
        if (PosInBlock == std::string::npos)
          break;

        Lines.emplace_back(ranges::stringRange(
          Output.begin() + LastPosition + 1, Output.begin() + PosInBlock));
      }
      if (Lines.back().substr(0, 2) == "0x" || Lines.back().empty())
        Lines.pop_back();

      std::string_view ExpectedAddress =
        SO.Arguments.at(AddressesBeginAt + CurrentFrame);
      Address = Address.substr(Address.size() - ExpectedAddress.size());
      if (Address != ExpectedAddress)
        LOG(warn)
          << "When reading 'addr2line' output, expected information for ["
          << ExpectedAddress << "] but got [" << Address
          << "] instead. Stack trace output might be corrupted.";

      // For all subsequent entries in LineOffsets, each (X, Y) pair refers to
      // a symbol name and a location for the current frame.
      Backtrace::Symbol Symbol{};
      Backtrace::Symbol* CurrentSymbol = nullptr;
      while (!Lines.empty())
      {
        if (!CurrentSymbol)
          CurrentSymbol = &Symbol;
        else
          // Recursively start storing the inlining information...
          CurrentSymbol = &CurrentSymbol->startInlineInfo();

        // The first line in the queue refers to a symbol name.
        std::string_view Name = Lines.front();
        Lines.erase(Lines.begin());
        std::string_view Location = Lines.front();
        Lines.erase(Lines.begin());

        const bool IsValidName = Name != "??";
        const bool IsValidLocation = Location != "??:?" && Location != "??:0";
        if (!IsValidName && !IsValidLocation)
          continue;

        if (IsValidName)
          CurrentSymbol->Name = Name;
        if (IsValidLocation)
        {
          auto FileLineSeparatorColon = Location.find_last_of(':');
          CurrentSymbol->Filename = Location.substr(0, FileLineSeparatorColon);
          if (FileLineSeparatorColon != std::string_view::npos)
          {
            std::string LineFragment{
              Location.substr(FileLineSeparatorColon + 1)};
            if (LineFragment != "?")
              CurrentSymbol->Line = std::stoull(LineFragment);
          }
        }
      }

      if (Symbol.hasMeaningfulInformation())
        Result.emplace_back(std::move(Symbol));
      else
        Result.emplace_back(std::nullopt);

      Output.erase(0, Pos + 1);
      ++CurrentFrame;
    } while (Pos != std::string::npos);
  } while (!Output.empty());

  return Result;
}

std::vector<std::optional<Backtrace::Symbol>>
LLVMSymbolizer::symbolise(const std::string& Object,
                          const std::vector<Backtrace::Frame*>& Frames) const
{
  using namespace monomux::system::unix;

  std::vector<std::optional<Backtrace::Symbol>> Result;
  Result.reserve(Frames.size());

  Process::SpawnOptions SO;
  SO.Program = name();
  SO.Arguments.emplace_back("--obj");
  SO.Arguments.emplace_back(Object);
  SO.Arguments.emplace_back("--output-style=LLVM");
  SO.Arguments.emplace_back("--addresses");
  SO.Arguments.emplace_back("--functions");
  SO.Arguments.emplace_back("--demangle");
  SO.Arguments.emplace_back("--inlines");
  const std::size_t AddressesBeginAt = SO.Arguments.size();

  for (Backtrace::Frame* F : Frames)
  {
    std::ostringstream OS;

    if (F->ImageOffset)
      OS << F->ImageOffset;
    else if (!F->Data.Offset.empty())
      OS << F->Data.Offset;
    else if (!F->Data.HexAddress.empty())
      OS << F->Address;
    else
      OS << "0x0";

    // llvm-symbolizer eats hex addresses with "0x" prefix like no problem.
    SO.Arguments.emplace_back(OS.str());
  }

  Pipe::AnonymousPipe OutPipe = Pipe::create(true);
  Pipe::AnonymousPipe ErrPipe = Pipe::create(true);
  static_cast<class Pipe*>(OutPipe.getRead())->setNonblocking();
  SO.StandardInput.emplace(fd::Traits::Invalid);
  SO.StandardOutput.emplace(OutPipe.getWrite()->raw());
  SO.StandardError.emplace(ErrPipe.getWrite()->raw());

  std::unique_ptr<system::Process> P = Process::spawn(SO);
  P->wait();

  std::string Output;
  std::size_t CurrentFrame = 0;
  do
  {
    Output.append(OutPipe.getRead()->read(BinaryPipeCommunicationSize));

    // The output format is:
    //
    //    address for symbol 1
    //    name for symbol 1
    //    location for symbol 1
    //    inlining symbol A for symbol 1        <- Optional.
    //    location for inlining symbol A        <- Optional.
    //    ... more inlining locations ...       <- Optional.
    //                                          <- Explicit newline.
    //    address for symbol 2
    //    name for symbol 2
    //    location for symbol 2
    //
    //    ...

    auto Pos = std::string::npos;
    do
    {
      // The lines in the symboliser output corresponding to the current frame.
      std::vector<std::string_view> Lines;

      // Find a block of lines separated by two explicit newlines. This
      // corresponds to the information about one stack frame address.
      Pos = Output.find("\n\n");
      if (Pos == std::string::npos)
        break;

      std::string::size_type LastPosition = 0;
      std::string::size_type PosInBlock = Output.find('\n', 1);
      std::string_view Address =
        ranges::stringRange(Output.begin(), Output.begin() + PosInBlock);
      while (PosInBlock <= Pos)
      {
        LastPosition = PosInBlock;
        PosInBlock = Output.find('\n', LastPosition + 1);
        Lines.emplace_back(ranges::stringRange(
          Output.begin() + LastPosition + 1, Output.begin() + PosInBlock));
      }
      if (Lines.back().empty())
        Lines.pop_back();

      std::string_view ExpectedAddress =
        SO.Arguments.at(AddressesBeginAt + CurrentFrame);
      if (Address != ExpectedAddress)
        LOG(warn)
          << "When reading 'llvm-symbolizer' output, expected information for ["
          << ExpectedAddress << "] but got [" << Address
          << "] instead. Stack trace output might be corrupted.";

      // For all subsequent entries in LineOffsets, each (X, Y) pair refers to
      // a symbol name and a location for the current frame.
      Backtrace::Symbol Symbol{};
      Backtrace::Symbol* CurrentSymbol = nullptr;
      while (!Lines.empty())
      {
        if (!CurrentSymbol)
          CurrentSymbol = &Symbol;
        else
          // Recursively start storing the inlining information...
          CurrentSymbol = &CurrentSymbol->startInlineInfo();

        // The first line in the queue refers to a symbol name.
        std::string_view Name = Lines.front();
        Lines.erase(Lines.begin());
        std::string_view Location = Lines.front();
        Lines.erase(Lines.begin());

        const bool IsValidName = Name != "??";
        const bool IsValidLocation = Location != "??:0:0";
        if (!IsValidName && !IsValidLocation)
          continue;

        if (IsValidName)
          CurrentSymbol->Name = Name;
        if (IsValidLocation)
        {
          auto LineColSeparatorColon = Location.find_last_of(':');
          auto FileLineSeparatorColon =
            Location.find_last_of(':', LineColSeparatorColon - 1);
          CurrentSymbol->Filename = Location.substr(0, FileLineSeparatorColon);
          if (FileLineSeparatorColon != std::string_view::npos)
          {
            CurrentSymbol->Line = std::stoull(std::string{
              Location.substr(FileLineSeparatorColon + 1,
                              LineColSeparatorColon - FileLineSeparatorColon)});

            if (LineColSeparatorColon != std::string_view::npos)
            {
              CurrentSymbol->Column = std::stoull(
                std::string{Location.substr(LineColSeparatorColon + 1)});
            }
          }
        }
      }

      if (Symbol.hasMeaningfulInformation())
        Result.emplace_back(std::move(Symbol));
      else
        Result.emplace_back(std::nullopt);

      Output.erase(0, Pos + 2);
      ++CurrentFrame;
    } while (Pos != std::string::npos);
  } while (!Output.empty());

  return Result;
}

} // namespace

std::vector<std::unique_ptr<Symboliser>> makeSymbolisers()
{
  std::vector<std::unique_ptr<Symboliser>> R;

#define SYMBOLISER(NAME)                                                       \
  {                                                                            \
    auto NAME##Ptr = std::make_unique<NAME>();                                 \
    if (NAME##Ptr->check())                                                    \
      R.emplace_back(std::move(NAME##Ptr));                                    \
  }

  SYMBOLISER(LLVMSymbolizer);
  SYMBOLISER(Addr2Line);

#undef SYMBOLISER

  return R;
}

DefaultBacktraceFormatter::DefaultBacktraceFormatter(std::ostream& OS,
                                                     const Backtrace& Trace)
  : OS(OS), Trace(Trace),
    FrameDigitsBase10(log::Logger::digits(Trace.getFrames().size())),
    PrefixLen(1 + FrameDigitsBase10 + 1)
{}

void DefaultBacktraceFormatter::print()
{
  for (const Backtrace::Frame& F : ranges::reverse_view(Trace.getFrames()))
  {
    OS << '#' << std::setw(FrameDigitsBase10) << F.Index << ": ";
    if (F.Info)
      formatPrettySymbolData(F);
    else
    {
      OS << "    ";
      formatRawSymbolData(F.Data, true);
    }

    OS << std::endl;
  }
}

void DefaultBacktraceFormatter::prefix()
{
  std::size_t PLen = PrefixLen;
  if (IsPrintingInlineInfo)
    ++PLen;

  OS << std::string(PLen, ' ');
  if (IsPrintingInlineInfo)
    OS << '|';
}

void DefaultBacktraceFormatter::formatRawSymbolData(const Backtrace::RawData& D,
                                                    bool PrintName)
{
  if ((D.Symbol.empty() || !PrintName) && !D.Offset.empty())
    OS << '[' << D.Offset << ']';
  else if (!D.Symbol.empty() && PrintName)
  {
    OS << D.Symbol;
    if (!D.Offset.empty())
      OS << " + " << D.Offset;
  }
  else if (D.Symbol.empty() && D.Offset.empty())
    OS << '[' << D.HexAddress << ']';

  OS << " in " << D.Binary;

  if (!D.Symbol.empty() && !D.HexAddress.empty())
    OS << " [" << D.HexAddress << ']';
}

void DefaultBacktraceFormatter::formatPrettySymbolData(
  const Backtrace::Frame& F)
{
  const Backtrace::Symbol& S = *F.Info;
  const Backtrace::RawData& D = F.Data;

  if (S.InlinedBy)
  {
    formatPrettySymbolInliningData(*S.InlinedBy);
    OS << " |> ";
  }
  else
    OS << "   ";

  if (S.Name.empty())
    OS << D.Symbol;
  else
    OS << S.Name;

  if (S.Filename.empty())
  {
    if (!S.Name.empty() || !D.Symbol.empty())
      OS << ' ';
    formatRawSymbolData(D, false);
  }
  else
  {
    if (!D.Symbol.empty() && !D.Offset.empty())
      OS << " + " << D.Offset;

    OS << " in " << S.Filename;
    if (S.Line)
      OS << ", line " << S.Line;

    printSourceCode(S);
  }
}

// NOLINTNEXTLINE(misc-no-recursion)
void DefaultBacktraceFormatter::formatPrettySymbolInliningData(
  const Backtrace::Symbol& S)
{
  if (S.InlinedBy)
  {
    formatPrettySymbolInliningData(*S.InlinedBy);
    OS << " <> ";
  }
  else
    OS << "<| ";

  if (S.Name.empty())
    OS << "<unknown>";
  else
    OS << S.Name;

  if (S.Filename.empty())
    OS << " in ?";
  else
  {
    OS << " in " << S.Filename;
    if (S.Line)
      OS << ", line " << S.Line;

    IsPrintingInlineInfo = true;
    printSourceCode(S);
    IsPrintingInlineInfo = false;
  }

  OS << '\n';
  prefix();
}

void DefaultBacktraceFormatter::printSourceCode(const Backtrace::Symbol& S)
{
  if (S.Filename.empty() || !S.Line)
    return;

  std::ifstream File{S.Filename, std::ios::in};
  if (!File.is_open())
    return;

  OS << std::endl;
  prefix();
  OS << std::endl;
  prefix();

  // Offer this many lines of context before and after the line reported by
  // the symboliser.
  static constexpr std::size_t Context = 3;
  const auto MinRequestedLineNo = [Target = S.Line]() -> std::size_t {
    if (Target <= Context)
      return 1;
    return Target - Context;
  }();
  const auto MaxRequestedLineNo = [Target = S.Line]() -> std::size_t {
    return Target + Context;
  }();
  const std::size_t MaxLineNoDigits = log::Logger::digits(MaxRequestedLineNo);

  std::istreambuf_iterator<char> Reader{File};
  const std::istreambuf_iterator<char> End{};
  std::size_t CurrentLineNo = 1;
  std::size_t CurrentColumnNo = 1;

  auto PrintLineNumber =
    [this, Target = S.Line, MaxLineNoDigits](std::size_t N) {
      OS << "  " << (N == Target ? ">>>" : "   ") << std::setw(MaxLineNoDigits)
         << N << ": ";
    };

  if (MinRequestedLineNo == 1)
    // The logic in the for-loop would not handle the first line because files
    // do not usually start with an empty line.
    PrintLineNumber(1);

  std::size_t ApproximatedTokenUndersquigglyLength = 0;
  bool TokenApproximated = false;
  for (; Reader != End; ++Reader)
  {
    const bool PrintingThisLine = CurrentLineNo >= MinRequestedLineNo &&
                                  CurrentLineNo <= MaxRequestedLineNo;
    if (*Reader == '\n')
    {
      if (PrintingThisLine)
      {
        // At the end of the current printed line, make sure the output is
        // line-fed.
        OS << '\n';
        prefix();

        if (S.Column && CurrentLineNo == S.Line)
        {
          OS << "  >>> ^" << std::string(MaxLineNoDigits - 2, '~') << "^ "
             << std::string(S.Column - 1, ' ') << '^';
          if (TokenApproximated && ApproximatedTokenUndersquigglyLength)
            OS << std::string(ApproximatedTokenUndersquigglyLength - 1, '~');

          OS << '\n';
          prefix();
        }
      }

      ++CurrentLineNo;

      if (CurrentLineNo >= MinRequestedLineNo &&
          CurrentLineNo <= MaxRequestedLineNo)
        // If the next line would still be printed, make a header.
        PrintLineNumber(CurrentLineNo);

      continue;
    }
    if (!PrintingThisLine)
      continue;

    OS << *Reader;
    if (S.Column && CurrentLineNo == S.Line && !TokenApproximated)
    {
      if (CurrentColumnNo >= S.Column)
        switch (*Reader)
        {
          case '.':
          case ',':
          case ':':
          case ';':
          case '-':
          case '?':
          case '(':
          case ')':
          case '[':
          case ']':
          case '{':
          case '}':
          case ' ':
          case '\t':
          case '\n':
            TokenApproximated = true;
            break;
          default:
            ++ApproximatedTokenUndersquigglyLength;
            break;
        }

      ++CurrentColumnNo;
    }
  }
}

} // namespace monomux::system::unix

#undef LOG
