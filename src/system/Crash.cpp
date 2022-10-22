/**
 * Copyright (C) 2022 Whisperity
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <dlfcn.h>
#include <execinfo.h>

#include "monomux/adt/POD.hpp"
#include "monomux/system/CheckedPOSIX.hpp"
#include "monomux/system/Pipe.hpp"
#include "monomux/system/Process.hpp"

#include "monomux/system/Crash.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Crash")

namespace monomux
{

namespace
{

constexpr std::size_t BinaryPipeCommunicationSize = 4096;

/// Creates a \p string_view from a range of \p string elements.
/// This constructor is only standardised starting C++20...
std::string_view stringRange(std::string::iterator B, std::string::iterator E)
{
  return {&*B, static_cast<std::string_view::size_type>(std::distance(B, E))};
}

/// Abstract interface for tools that are capable of giving symbol info about a
/// process at run-time.
struct Symboliser
{
  const std::string& name() const noexcept { return Binary; }

  bool check() const;

  virtual std::vector<std::optional<Backtrace::Symbol>>
  symbolise(const std::string& Object,
            const std::vector<Backtrace::Frame*>& Frames) const = 0;

  virtual ~Symboliser() = default;

  static void noSymbolisersMessage();

protected:
  std::string Binary;
  Symboliser(std::string Binary) : Binary(std::move(Binary)) {}
};

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

auto makeSymbolisers()
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

void Symboliser::noSymbolisersMessage()
{
  MONOMUX_TRACE_LOG(
    LOG(warn) << "No symboliser found. Please install 'binutils' for addr2line "
                 "or 'llvm-symbolizer', or ensure they are in the PATH!");
}

bool Symboliser::check() const
{
  Process::SpawnOptions SO;
  SO.Program = Binary;
  SO.Arguments.emplace_back("--help");

  Pipe::AnonymousPipe Pipe = Pipe::create(true);
  Pipe::AnonymousPipe PipeErr = Pipe::create(true);
  Pipe.getRead()->setNonblocking();
  SO.StandardInput.emplace(fd::Invalid);
  SO.StandardOutput.emplace(Pipe.getWrite()->raw());
  SO.StandardError.emplace(PipeErr.getWrite()->raw());

  Process P = Process::spawn(SO);
  P.wait();

  std::string Out = Pipe.getRead()->read(BinaryPipeCommunicationSize);
  return !Out.empty();
}

std::vector<std::optional<Backtrace::Symbol>>
Addr2Line::symbolise(const std::string& Object,
                     const std::vector<Backtrace::Frame*>& Frames) const
{
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
  OutPipe.getRead()->setNonblocking();
  SO.StandardInput.emplace(fd::Invalid);
  SO.StandardOutput.emplace(OutPipe.getWrite()->raw());
  SO.StandardError.emplace(ErrPipe.getWrite()->raw());

  Process P = Process::spawn(SO);
  P.wait();

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
        stringRange(Output.begin(), Output.begin() + PosInBlock);
      while (PosInBlock <= Pos)
      {
        LastPosition = PosInBlock;
        PosInBlock = Output.find('\n', LastPosition + 1);
        if (PosInBlock == std::string::npos)
          break;

        Lines.emplace_back(stringRange(Output.begin() + LastPosition + 1,
                                       Output.begin() + PosInBlock));
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
  OutPipe.getRead()->setNonblocking();
  SO.StandardInput.emplace(fd::Invalid);
  SO.StandardOutput.emplace(OutPipe.getWrite()->raw());
  SO.StandardError.emplace(ErrPipe.getWrite()->raw());

  Process P = Process::spawn(SO);
  P.wait();

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
        stringRange(Output.begin(), Output.begin() + PosInBlock);
      while (PosInBlock <= Pos)
      {
        LastPosition = PosInBlock;
        PosInBlock = Output.find('\n', LastPosition + 1);
        Lines.emplace_back(stringRange(Output.begin() + LastPosition + 1,
                                       Output.begin() + PosInBlock));
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

/// Performs the mental gymnastics required to map the locations of where a
/// stack frame is loaded into memory (and thus available in a backtrace) to
/// locations in the image of shared library found on the disk and read in situ.
///
/// This is needed because a symboliser reading a binary on the disk does not
/// care about the magic offset of the loaded library which is only available in
/// the currently backtracing executable's memory map.
void putSharedObjectOffsets(const std::vector<Backtrace::Frame*>& Frames)
{
  if (Frames.empty())
    return;

  std::string BinaryStr{Frames.front()->Data.Binary};
  auto MaybeObj = CheckedPOSIX(
    [&BinaryStr] { return ::dlopen(BinaryStr.c_str(), RTLD_LAZY); }, nullptr);
  if (!MaybeObj)
  {
    MONOMUX_TRACE_LOG(LOG(warn)
                      << "dlopen(): failed to generate symbol info for '"
                      << BinaryStr << "': " << dlerror());
  }

  std::vector<void*> OffsetPtrs;
  OffsetPtrs.reserve(Frames.size());
  for (Backtrace::Frame* F : Frames)
  {
    void* Offset = nullptr;
    // There can be two kinds of symbols in the dump.
    if (F->Data.Offset.empty())
    {
      // The first is statically linked into (usually) the current binary, in
      // which case addr2line can natively resolve it properly.
      MONOMUX_TRACE_LOG(LOG(data)
                        << '#' << F->Index << ": Empty 'Offset' field.");
      std::istringstream AddressBuf;
      AddressBuf.str(std::string{F->Data.HexAddress});
      AddressBuf >> Offset;
    }
    else
    {
      // The other kind of symbols come from shared libraries and must be
      // resolved via first figuring out where the shared library and the
      // symbol is loaded, because the hex address is only valid for the current
      // binary (where the dynamic image is loaded), and not for someone
      // accessing the object from the outside.
      if (!MaybeObj)
        continue;

      {
        std::istringstream OffsetBuf;
        OffsetBuf.str(std::string{F->Data.Offset});
        OffsetBuf >> Offset;
      }

      const void* Address = nullptr;
      if (!F->Data.Symbol.empty())
      {
        std::string SymbolStr{F->Data.Symbol};
        auto MaybeAddr =
          CheckedPOSIX([Obj = MaybeObj.get(),
                        &SymbolStr] { return ::dlsym(Obj, SymbolStr.c_str()); },
                       nullptr);
        if (!MaybeAddr)
        {
          MONOMUX_TRACE_LOG(LOG(debug)
                            << "dlsym(): failed to generate symbol info for #"
                            << F->Index << ": " << dlerror());
          continue;
        }
        Address = MaybeAddr.get();
        MONOMUX_TRACE_LOG(LOG(data) << "Address of '" << F->Data.Symbol
                                    << "' in memory is " << Address);

        POD<::Dl_info> SymbolInfo;
        {
          auto SymInfoError = CheckedPOSIX(
            [Address, &SymbolInfo] { return ::dladdr(Address, &SymbolInfo); },
            0);
          if (!SymInfoError)
          {
            MONOMUX_TRACE_LOG(
              LOG(warn) << "dladdr(): failed to generate symbol info for #"
                        << F->Index << ": " << dlerror());
            continue;
          }
        }

        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        Offset = reinterpret_cast<void*>(
          (reinterpret_cast<std::uintptr_t>(SymbolInfo->dli_saddr) -
           reinterpret_cast<std::uintptr_t>(SymbolInfo->dli_fbase)) +
          reinterpret_cast<std::uintptr_t>(Offset));
        MONOMUX_TRACE_LOG(LOG(data) << "Offset of '" << F->Data.Symbol
                                    << "' in shared library is " << Offset);
      }
      else
      {
        MONOMUX_TRACE_LOG(LOG(data)
                          << "Offset " << Offset << " without a symbol name.");
      }
    }

    F->ImageOffset = Offset;
  }

  if (MaybeObj)
    CheckedPOSIX([DLObj = MaybeObj.get()](bool& CloseError) {
      CloseError = (::dlclose(DLObj) != 0);
    });
}

} // namespace

// NOLINTNEXTLINE(misc-no-recursion)
void Backtrace::Symbol::mergeFrom(Symbol&& RHS)
{
  if (Name.empty() && !RHS.Name.empty())
    Name = std::move(RHS.Name);
  if (Filename.empty() && !RHS.Filename.empty())
    Filename = std::move(RHS.Filename);
  if (!Line)
    Line = RHS.Line;
  if (!Column)
    Column = RHS.Column;

  if (!InlinedBy && RHS.InlinedBy)
    InlinedBy = std::move(RHS.InlinedBy);
  else if (InlinedBy && RHS.InlinedBy)
    InlinedBy->mergeFrom(std::move(*RHS.InlinedBy));
}

Backtrace::Symbol& Backtrace::Symbol::startInlineInfo()
{
  if (!InlinedBy)
    InlinedBy = std::make_unique<Symbol>(Symbol{});
  return *InlinedBy;
}

// NOLINTNEXTLINE(misc-no-recursion)
bool Backtrace::Symbol::hasMeaningfulInformation() const
{
  return !Name.empty() || !Filename.empty() ||
         (InlinedBy && InlinedBy->hasMeaningfulInformation());
}

Backtrace::Backtrace(std::size_t Depth, std::size_t Ignored)
  : IgnoredFrameCount(Ignored), SymbolDataBuffer(nullptr)
{
  // If ignoring I frames and requesting N, we request (I+N) because backtrace()
  // only works from the *current* stack frame.
  Depth += Ignored;
  if (Depth > MaxSize)
  {
    MONOMUX_TRACE_LOG(LOG(debug)
                      << "Requested " << Depth
                      << " stack frames, which is larger than supported limit "
                      << MaxSize);
    Depth = MaxSize;
  }

  POD<void* [MaxSize]> Addresses;
  std::size_t FrameCount = 0;
  {
    auto MaybeFrameCount = CheckedPOSIX(
      [&Addresses, Depth] { return ::backtrace(Addresses, Depth); }, -1);
    if (!MaybeFrameCount)
    {
      LOG(error) << "backtrace() failed: " << MaybeFrameCount.getError() << ' '
                 << MaybeFrameCount.getError().message();
      return;
    }
    FrameCount = MaybeFrameCount.get();
    MONOMUX_TRACE_LOG(LOG(data) << "backtrace() returned " << FrameCount
                                << " symbols, ignoring " << Ignored);
    FrameCount -= Ignored;
    Frames.reserve(FrameCount);
  }

  {
    auto MaybeSymbols = CheckedPOSIX(
      [StartAddress = &Addresses[0] + Ignored, FrameCount] {
        return ::backtrace_symbols(StartAddress, FrameCount);
      },
      nullptr);
    if (!MaybeSymbols)
      LOG(error) << "backtrace_symbols() failed: " << MaybeSymbols.getError()
                 << ' ' << MaybeSymbols.getError().message();
    else
      SymbolDataBuffer = MaybeSymbols.get();
  }

  for (std::size_t I = IgnoredFrameCount; I < FrameCount; ++I)
  {
    Frame F{};
    F.Index = FrameCount - 1 - I;
    F.Address = Addresses[I];
    F.Data.Full = SymbolDataBuffer[I];

    const auto OpenParen = F.Data.Full.find('(');
    const auto Plus = F.Data.Full.find('+', OpenParen);
    const auto CloseParen = F.Data.Full.find(')', OpenParen);
    const auto OpenSquare = F.Data.Full.find('[');
    const auto CloseSquare = F.Data.Full.find(']', OpenSquare);

    F.Data.Binary = F.Data.Full.substr(0, OpenParen);
    if (Plus != std::string_view::npos && (Plus - OpenParen) > 0)
    {
      F.Data.Symbol = F.Data.Full.substr(OpenParen + 1, Plus - OpenParen - 1);
      F.Data.Offset = F.Data.Full.substr(Plus + 1, CloseParen - Plus - 1);
    }
    F.Data.HexAddress =
      F.Data.Full.substr(OpenSquare + 1, CloseSquare - OpenSquare - 1);

    Frames.emplace_back(std::move(F));
  }
}

Backtrace::~Backtrace()
{
  if (SymbolDataBuffer)
  {
#ifndef NDEBUG
    // Freeing the buffer for the symbol data invalidates the views into it.
    std::for_each(Frames.begin(), Frames.end(), [](Frame& F) {
      F.Data.Full = {};
      F.Data.HexAddress = {};
      F.Data.Binary = {};
      F.Data.Symbol = {};
      F.Data.Offset = {};
    });
#endif
    std::free(const_cast<char**>(SymbolDataBuffer));
    SymbolDataBuffer = nullptr;
  }
}

void Backtrace::prettify()
{
  std::vector<std::unique_ptr<Symboliser>> Symbolisers = makeSymbolisers();
  if (Symbolisers.empty())
  {
    MONOMUX_TRACE_LOG(Symboliser::noSymbolisersMessage());
    return;
  }

  std::map<std::string, std::vector<Frame*>> FramesPerBinary;
  for (Frame& F : Frames)
  {
    if (F.Info)
      // Already prettified.
      return;
    if (!F.Address || F.Data.Full.empty())
      // Nothing to prettify.
      return;

    auto& FramesForBinaryOfThisFrame =
      FramesPerBinary
        .try_emplace(std::string{F.Data.Binary}, std::vector<Frame*>{})
        .first->second;
    FramesForBinaryOfThisFrame.emplace_back(&F);
  }

  for (auto& P : FramesPerBinary)
  {
    MONOMUX_TRACE_LOG(LOG(data) << "Prettifying " << P.second.size()
                                << " stack frames from '" << P.first << "'");
    const std::string& Object = P.first;
    const auto& Frames = P.second;
    putSharedObjectOffsets(Frames);

    auto SymbolisersForThisObject = [&Symbolisers] {
      std::vector<Symboliser*> SymbPtrs;
      std::transform(Symbolisers.begin(),
                     Symbolisers.end(),
                     std::back_inserter(SymbPtrs),
                     [](auto&& SPtr) { return SPtr.get(); });
      return SymbPtrs;
    }();
    while (!SymbolisersForThisObject.empty())
    {
      Symboliser* Symboliser = SymbolisersForThisObject.back();
      MONOMUX_TRACE_LOG(LOG(trace)
                        << "Trying symboliser '" << Symboliser->name()
                        << "' for '" << Object << "'...");

      std::vector<std::optional<Symbol>> Symbols =
        Symboliser->symbolise(Object, Frames);
      for (std::size_t I = 0; I < Frames.size(); ++I)
      {
        if (I >= Symbols.size())
          continue;
        if (!Symbols.at(I))
        {
          // Some symbolisers, like 'llvm-symbolizer' might be unable to
          // symbolize certain GLIBC or LIBSTDC++ constructs which others, like
          // 'addr2line' has been observed to handle properly. In case we find
          // that it could not, let's try the next symboliser.
          continue;
        }

        if (!Frames.at(I)->Info)
          Frames.at(I)->Info = std::move(*Symbols.at(I));
        else
          Frames.at(I)->Info->mergeFrom(std::move(*Symbols.at(I)));
      }

      SymbolisersForThisObject.pop_back();
    }
  }
}

namespace
{

namespace ranges
{

template <typename T> struct ReverseView
{
  T& Collection;
};

template <typename T> auto begin(ReverseView<T> I)
{
  using std::rbegin;
  return rbegin(I.Collection);
}

template <typename T> auto end(ReverseView<T> I)
{
  using std::rend;
  return rend(I.Collection);
}

// NOLINTNEXTLINE(readability-identifier-naming)
template <typename T> ReverseView<T> reverse_view(T&& Collection)
{
  return {Collection};
}

} // namespace ranges

/// Implement a default backtrace formatting logic that pretty-prints the
/// frames of a \p Backtrace in an orderly, numbered fashion.
class DefaultBacktraceFormatter
{
public:
  DefaultBacktraceFormatter(std::ostream& OS, const Backtrace& Trace)
    : OS(OS), Trace(Trace),
      FrameDigitsBase10(log::Logger::digits(Trace.getFrames().size())),
      PrefixLen(1 + FrameDigitsBase10 + 1)
  {}

  void print()
  {
    for (const Backtrace::Frame& F : ranges::reverse_view(Trace.getFrames()))
    {
      OS << '#' << std::setw(FrameDigitsBase10) << F.Index << ": ";
      if (F.Info)
        formatPrettySymbolData(F);
      else
        formatRawSymbolData(F.Data);

      OS << std::endl;
    }
  }

private:
  std::ostream& OS;
  const Backtrace& Trace;

  const std::size_t FrameDigitsBase10;

  /// The size of the prefix printed before each frame.
  std::size_t PrefixLen;

  bool IsPrintingInlineInfo = false;

  void prefix()
  {
    std::size_t PLen = PrefixLen;
    if (IsPrintingInlineInfo)
      ++PLen;

    OS << std::string(PLen, ' ');
    if (IsPrintingInlineInfo)
      OS << '|';
  }

  void formatRawSymbolData(const Backtrace::RawData& D)
  {
    OS << "   ";
    if (D.Symbol.empty() && !D.Offset.empty())
      OS << '[' << D.Offset << ']';
    else if (!D.Symbol.empty())
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

  void formatPrettySymbolData(const Backtrace::Frame& F)
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
      OS << " in " << D.Binary;
    else
    {
      OS << " in " << S.Filename;
      if (S.Line)
        OS << ", line " << S.Line;

      printSourceCode(S);
    }
  }

  // NOLINTNEXTLINE(misc-no-recursion)
  void formatPrettySymbolInliningData(const Backtrace::Symbol& S)
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

  void printSourceCode(const Backtrace::Symbol& S)
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
        OS << "  " << (N == Target ? ">>>" : "   ")
           << std::setw(MaxLineNoDigits) << N << ": ";
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
        char C = *Reader;
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
};


} // namespace

void printBacktrace(std::ostream& OS, const Backtrace& Trace)
{
  OS << "Stack trace (most recent call last):\n";
  if (Trace.IgnoredFrameCount > 0)
    OS << "! " << Trace.IgnoredFrameCount << " frames ignored\n";
  OS << '\n';
  DefaultBacktraceFormatter{OS, Trace}.print();
}

void printBacktrace(std::ostream& OS, bool Prettify)
{
  Backtrace BT;
  if (Prettify)
    BT.prettify();
  printBacktrace(OS, BT);
}

} // namespace monomux

#undef LOG
