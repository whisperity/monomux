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
#include <map>
#include <sstream>
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

/// Performs the mental gymnastics required to map the locations of where a
/// stack frame is loaded into memory (and thus available in a backtrace) to
/// locations in the image of shared library found on the disk and read in situ.
void putSharedObjectOffsets(const std::vector<Backtrace::Frame*>& Frames)
{
  if (Frames.empty())
    return;

  std::string BinaryStr{Frames.front()->Binary};
  auto MaybeObj = CheckedPOSIX(
    [&BinaryStr] { return ::dlopen(BinaryStr.c_str(), RTLD_LAZY); }, nullptr);
  if (!MaybeObj)
  {
    MONOMUX_TRACE_LOG(LOG(debug)
                      << "dlopen(): failed to generate symbol info for '"
                      << BinaryStr << "': " << dlerror());
  }

  std::vector<void*> OffsetPtrs;
  OffsetPtrs.reserve(Frames.size());
  for (Backtrace::Frame* F : Frames)
  {
    void* Offset = nullptr;
    // There can be two kinds of symbols in the dump.
    if (F->Offset.empty())
    {
      // The first is statically linked into (usually) the current binary, in
      // which case addr2line can natively resolve it properly.
      MONOMUX_TRACE_LOG(LOG(data)
                        << '#' << F->Index << ": Empty 'Offset' field.");
      std::istringstream AddressBuf;
      AddressBuf.str(std::string{F->HexAddress});
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
        OffsetBuf.str(std::string{F->Offset});
        OffsetBuf >> Offset;
      }

      const void* Address = nullptr;
      if (!F->Symbol.empty())
      {
        std::string SymbolStr{F->Symbol};
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
        MONOMUX_TRACE_LOG(LOG(data) << "Address of '" << F->Symbol
                                    << "' in memory is " << Address);

        POD<::Dl_info> SymbolInfo;
        {
          auto SymInfoError = CheckedPOSIX(
            [Address, &SymbolInfo] { return ::dladdr(Address, &SymbolInfo); },
            0);
          if (!SymInfoError)
          {
            MONOMUX_TRACE_LOG(
              LOG(debug) << "dladdr(): failed to generate symbol info for #"
                         << F->Index << ": " << dlerror());
            continue;
          }
        }

        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        Offset = reinterpret_cast<void*>(
          (reinterpret_cast<std::uintptr_t>(SymbolInfo->dli_saddr) -
           reinterpret_cast<std::uintptr_t>(SymbolInfo->dli_fbase)) +
          reinterpret_cast<std::uintptr_t>(Offset));
        MONOMUX_TRACE_LOG(LOG(data) << "Offset of '" << F->Symbol
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

struct Symboliser
{
  const std::string& name() const noexcept { return Binary; }

  bool check() const;

  virtual std::vector<std::string>
  symbolise(const std::string& Object,
            const std::vector<Backtrace::Frame*>& Frames) = 0;

  virtual bool
  isInvalidSymbolisationResult(const std::string& S) const noexcept = 0;

  virtual ~Symboliser() = default;

  static void noSymbolisersMessage();

protected:
  std::string Binary;
  Symboliser(std::string Binary) : Binary(std::move(Binary)) {}
};

struct Addr2Line : public Symboliser
{
  Addr2Line() : Symboliser("addr2line") {}

  std::vector<std::string>
  symbolise(const std::string& Object,
            const std::vector<Backtrace::Frame*>& Frames) override;

  bool
  isInvalidSymbolisationResult(const std::string& S) const noexcept override
  {
    return S == "?? ??:0";
  }
};

struct LLVMSymbolizer : public Symboliser
{
  LLVMSymbolizer() : Symboliser("llvm-symbolizer") {}

  std::vector<std::string>
  symbolise(const std::string& Object,
            const std::vector<Backtrace::Frame*>& Frames) override;

  bool
  isInvalidSymbolisationResult(const std::string& S) const noexcept override
  {
    return S == "?? at ??:0:0";
  }
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

  SYMBOLISER(Addr2Line);
  SYMBOLISER(LLVMSymbolizer);

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

std::vector<std::string>
Addr2Line::symbolise(const std::string& Object,
                     const std::vector<Backtrace::Frame*>& Frames)
{
  std::vector<std::string> Result;
  Result.reserve(Frames.size());

  Process::SpawnOptions SO;
  SO.Program = name();
  SO.Arguments.emplace_back("-e"); // Specify binary to read.
  SO.Arguments.emplace_back(Object);
  SO.Arguments.emplace_back("--pretty-print");
  SO.Arguments.emplace_back("--functions");
  SO.Arguments.emplace_back("--demangle");

  for (Backtrace::Frame* F : Frames)
  {
    if (F->ImageOffset)
    {
      std::ostringstream OS;
      OS << F->ImageOffset;
      // Strip "0x"
      SO.Arguments.emplace_back(OS.str().substr(2));
    }
    else
      SO.Arguments.emplace_back("0x0");
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
  do
  {
    Output.append(OutPipe.getRead()->read(BinaryPipeCommunicationSize));

    auto Pos = std::string::npos;
    do
    {
      Pos = Output.find('\n');
      if (Pos == std::string::npos)
        break;
      Result.emplace_back(Output.substr(0, Pos));
      Output.erase(0, Pos + 1);
    } while (Pos != std::string::npos);
  } while (!Output.empty());

  return Result;
}

std::vector<std::string>
LLVMSymbolizer::symbolise(const std::string& Object,
                          const std::vector<Backtrace::Frame*>& Frames)
{
  std::vector<std::string> Result;
  Result.reserve(Frames.size());

  Process::SpawnOptions SO;
  SO.Program = name();
  SO.Arguments.emplace_back("--obj");
  SO.Arguments.emplace_back(Object);
  SO.Arguments.emplace_back("--pretty-print");
  SO.Arguments.emplace_back("--functions");
  SO.Arguments.emplace_back("--demangle");

  for (Backtrace::Frame* F : Frames)
  {
    if (F->ImageOffset)
    {
      std::ostringstream OS;
      OS << F->ImageOffset;
      // llvm-symbolizer eats hex addresses with "0x" prefix like no problem.
      SO.Arguments.emplace_back(OS.str());
    }
    else
      SO.Arguments.emplace_back("0x0");
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
  do
  {
    Output.append(OutPipe.getRead()->read(BinaryPipeCommunicationSize));

    auto Pos = std::string::npos;
    do
    {
      Pos = Output.find("\n\n");
      if (Pos == std::string::npos)
        break;
      Result.emplace_back(Output.substr(0, Pos));
      Output.erase(0, Pos + 2);
    } while (Pos != std::string::npos);
  } while (!Output.empty());

  return Result;
}

} // namespace

Backtrace::Backtrace(std::size_t Depth, std::size_t Ignored)
  : SymbolDataBuffer(nullptr)
{
  // If ignoring I frames and requesting N, we request (I+N) because backtrace()
  // only works from the *current* stack frame.
  Depth += Ignored;
  if (Depth > MaxSize)
  {
    LOG(debug) << "Requested " << Depth
               << " stack frames, which is larger than supported limit "
               << MaxSize;
    Depth = MaxSize;
  }

  POD<void* [MaxSize]> Addresses;
  std::size_t FrameCount = 0;
  {
    auto MaybeFrameCount = CheckedPOSIX(
      [&Addresses, Depth] { return ::backtrace(Addresses, Depth); }, -1);
    if (!MaybeFrameCount)
    {
      LOG(error) << "backtrace() failed: " << MaybeFrameCount.getError();
      return;
    }
    FrameCount = MaybeFrameCount.get();
    LOG(trace) << "backtrace() returned " << FrameCount << " symbols, ignoring "
               << Ignored;
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
      LOG(error) << "backtrace_symbols() failed: " << MaybeSymbols.getError();
    else
      SymbolDataBuffer = MaybeSymbols.get();
  }

  for (std::size_t I = Ignored; I < FrameCount; ++I)
  {
    Frame F{};
    F.Index = I - Ignored;
    F.Address = Addresses[I];
    F.SymbolData = SymbolDataBuffer[I];

    const auto OpenParen = F.SymbolData.find('(');
    const auto Plus = F.SymbolData.find('+', OpenParen);
    const auto CloseParen = F.SymbolData.find(')', OpenParen);
    const auto OpenSquare = F.SymbolData.find('[');
    const auto CloseSquare = F.SymbolData.find(']', OpenSquare);

    F.Binary = F.SymbolData.substr(0, OpenParen);
    if (Plus != std::string_view::npos && (Plus - OpenParen) > 0)
    {
      F.Symbol = F.SymbolData.substr(OpenParen + 1, Plus - OpenParen - 1);
      F.Offset = F.SymbolData.substr(Plus + 1, CloseParen - Plus - 1);
    }
    F.HexAddress =
      F.SymbolData.substr(OpenSquare + 1, CloseSquare - OpenSquare - 1);

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
      F.SymbolData = {};
      F.HexAddress = {};
      F.Binary = {};
      F.Symbol = {};
      F.Offset = {};
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
    if (!F.Pretty.empty())
      // Already prettified.
      return;
    if (!F.Address || F.SymbolData.empty())
      // Nothing to prettify.
      return;

    auto& FramesForBinaryOfThisFrame =
      FramesPerBinary.try_emplace(std::string{F.Binary}, std::vector<Frame*>{})
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
    bool NeedsAnotherRoundOfSymbolisation = true;
    while (!SymbolisersForThisObject.empty() &&
           NeedsAnotherRoundOfSymbolisation)
    {
      Symboliser* Symboliser = SymbolisersForThisObject.back();
      MONOMUX_TRACE_LOG(LOG(trace)
                        << "Trying symboliser '" << Symboliser->name()
                        << "' for '" << Object << "'...");

      std::vector<std::string> Pretties = Symboliser->symbolise(Object, Frames);
      NeedsAnotherRoundOfSymbolisation = false; // Assume this round succeeds.
      for (std::size_t I = 0; I < Frames.size(); ++I)
      {
        if (I >= Pretties.size() || !Frames.at(I)->Pretty.empty())
          continue;
        if (!Symboliser->isInvalidSymbolisationResult(Pretties.at(I)))
        {
          Frames.at(I)->Pretty = std::move(Pretties.at(I));
          continue;
        }

        // Some symbolisers, like 'llvm-symbolizer' might be unable to symbolize
        // certain GLIBC or LIBSTDC++ constructs which others, like 'addr2line'
        // has been observed to handle properly.
        // In case we find that it could not, let's try the next symboliser.
        Frames.at(I)->Pretty.clear();
        NeedsAnotherRoundOfSymbolisation = true;
      }

      SymbolisersForThisObject.pop_back();
    }
  }
}

} // namespace monomux
