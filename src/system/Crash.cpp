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
    std::for_each(
      Frames.begin(), Frames.end(), [](Frame& F) { F.SymbolData = {}; });
#endif
    std::free(const_cast<char**>(SymbolDataBuffer));
    SymbolDataBuffer = nullptr;
  }
}

static constexpr std::size_t BinaryPipeCommunicationSize = 4096;

static bool tryBinary(const std::string& Binary)
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

/// Performs the mental gymnastics required to map the locations of where a
/// stack frame is loaded into memory (and thus available in a backtrace) to
/// locations in the image of shared library found on the disk and read in situ.
static std::vector<void*>
getSharedObjectOffsets(const std::vector<Backtrace::Frame*>& Frames)
{
  if (Frames.empty())
    return {};

  std::string BinaryStr{Frames.front()->Binary};
  auto MaybeObj = CheckedPOSIX(
    [&BinaryStr] { return ::dlopen(BinaryStr.c_str(), RTLD_LAZY); }, nullptr);
  if (!MaybeObj)
  {
    LOG(warn) << "dlopen(): failed to generate symbol info for '" << BinaryStr
              << "': " << dlerror();
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
      MONOMUX_TRACE_LOG(LOG(trace)
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
          LOG(warn) << "dlsym(): failed to generate symbol info for #"
                    << F->Index << ": " << dlerror();
          continue;
        }
        Address = MaybeAddr.get();
        MONOMUX_TRACE_LOG(LOG(trace) << "Address of '" << F->Symbol
                                     << "' in memory is " << Address);

        POD<::Dl_info> SymbolInfo;
        {
          auto SymInfoError = CheckedPOSIX(
            [Address, &SymbolInfo] { return ::dladdr(Address, &SymbolInfo); },
            0);
          if (!SymInfoError)
          {
            LOG(warn) << "dladdr(): failed to generate symbol info for #"
                      << F->Index << ": " << dlerror();
            continue;
          }
        }

        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        Offset = reinterpret_cast<void*>(
          (reinterpret_cast<std::uintptr_t>(SymbolInfo->dli_saddr) -
           reinterpret_cast<std::uintptr_t>(SymbolInfo->dli_fbase)) +
          reinterpret_cast<std::uintptr_t>(Offset));
        MONOMUX_TRACE_LOG(LOG(trace) << "Offset of ' " << F->Symbol
                                     << "' in shared library is " << Offset);
      }
      else
      {
        MONOMUX_TRACE_LOG(LOG(trace)
                          << "Offset " << Offset << " without a symbol name.");
      }
    }

    OffsetPtrs.emplace_back(Offset);
  }

  if (MaybeObj)
    CheckedPOSIX([DLObj = MaybeObj.get()](bool& CloseError) {
      CloseError = (::dlclose(DLObj) != 0);
    });

  return OffsetPtrs;
}

static std::vector<std::string>
addr2line(const std::string& Binary,
          const std::vector<Backtrace::Frame*>& Frames)
{
  std::vector<void*> FrameOffsets = getSharedObjectOffsets(Frames);
  std::vector<std::string> Result;
  Result.reserve(Frames.size());

  Process::SpawnOptions SO;
  SO.Program = "addr2line";
  SO.Arguments.emplace_back("-e"); // Specify binary to read.
  SO.Arguments.emplace_back(Binary);
  SO.Arguments.emplace_back("-p"); // Pretty printer.
  SO.Arguments.emplace_back("-f"); // Print functions.
  SO.Arguments.emplace_back("-C"); // Demangle.

  for (std::size_t I = 0; I < Frames.size(); ++I)
  {
    std::ostringstream OS;
    OS << FrameOffsets.at(I);
    // Strip "0x"
    std::string OffsetStrNoHexPrefix = OS.str().substr(2);
    SO.Arguments.emplace_back(OffsetStrNoHexPrefix);
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

static std::vector<std::string>
// NOLINTNEXTLINE(readability-identifier-naming)
llvm_symbolizer(const std::string& Binary,
                const std::vector<Backtrace::Frame*>& Frames)
{
  std::vector<void*> FrameOffsets = getSharedObjectOffsets(Frames);
  std::vector<std::string> Result;
  Result.reserve(Frames.size());

  Process::SpawnOptions SO;
  SO.Program = "llvm-symbolizer";
  SO.Arguments.emplace_back("-e"); // Specify binary to read.
  SO.Arguments.emplace_back(Binary);
  SO.Arguments.emplace_back("-p"); // Pretty printer.
  SO.Arguments.emplace_back("-f"); // Print functions.
  SO.Arguments.emplace_back("-C"); // Demangle.

  for (std::size_t I = 0; I < Frames.size(); ++I)
  {
    std::ostringstream OS;
    OS << FrameOffsets.at(I);
    // llvm-symbolizer eats hex addresses with "0x" prefix like no problem.
    std::string OffsetStr = OS.str();
    SO.Arguments.emplace_back(OffsetStr);
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

void Backtrace::prettify()
{
  std::string Symboliser;
  if (Symboliser.empty() && tryBinary("llvm-symbolizer"))
  {
    MONOMUX_TRACE_LOG(LOG(info) << "Using symboliser: LLVM symbolizer");
    Symboliser = "llvm-symbolizer";
  }
  if (Symboliser.empty() && tryBinary("addr2line"))
  {
    MONOMUX_TRACE_LOG(LOG(info) << "Using symboliser: GNU addr2line");
    Symboliser = "addr2line";
  }
  if (Symboliser.empty())
  {
    MONOMUX_TRACE_LOG(
      LOG(warn)
      << "No symboliser found. Please install 'binutils' for addr2line or "
         "'llvm-symbolizer', or ensure they are in the PATH!");
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
    MONOMUX_TRACE_LOG(LOG(trace) << "Prettifying " << P.second.size()
                                 << " stack frames from '" << P.first << "'");
    std::vector<std::string> Pretties;
    if (Symboliser == "llvm-symbolizer")
      Pretties = llvm_symbolizer(P.first, P.second);
    else if (Symboliser == "addr2line")
      Pretties = addr2line(P.first, P.second);
    for (std::size_t I = 0; I < P.second.size(); ++I)
    {
      if (I < Pretties.size())
        P.second.at(I)->Pretty = std::move(Pretties.at(I));
    }
  }
}

} // namespace monomux
