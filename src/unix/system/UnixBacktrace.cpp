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
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <dlfcn.h>
#include <execinfo.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/adt/POD.hpp"

#include "UnixBacktraceDetail.hpp"

#include "monomux/system/UnixBacktrace.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Backtrace")

namespace monomux::system
{

namespace unix
{

namespace
{

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
  auto MaybeObj = CheckedErrno(
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
          CheckedErrno([Obj = MaybeObj.get(),
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
          auto SymInfoError = CheckedErrno(
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
    CheckedErrno([DLObj = MaybeObj.get()](bool& CloseError) {
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
  : system::Backtrace(Ignored), SymbolDataBuffer(nullptr)
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
    auto MaybeFrameCount = CheckedErrno(
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
    auto MaybeSymbols = CheckedErrno(
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

} // namespace unix

void printBacktrace(std::ostream& OS, const Backtrace& Trace)
{
  OS << "Stack trace (most recent call last):\n";
  if (Trace.IgnoredFrameCount > 0)
    OS << "! " << Trace.IgnoredFrameCount << " frames ignored\n";
  OS << '\n';
  unix::DefaultBacktraceFormatter{OS,
                                  static_cast<const unix::Backtrace&>(Trace)}
    .print();
}

void printBacktrace(std::ostream& OS, bool Prettify)
{
  unix::Backtrace BT;
  if (Prettify)
    BT.prettify();
  printBacktrace(OS, BT);
}

} // namespace monomux::system

#undef LOG
