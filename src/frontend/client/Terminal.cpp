/* SPDX-License-Identifier: GPL-3.0-only */
#include <cassert>
#include <functional>
#include <sstream>

#include <sys/ioctl.h>
#include <termios.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/client/Client.hpp"
#include "monomux/system/Handle.hpp"
#include "monomux/system/Pipe.hpp"
#include "monomux/system/UnixPipe.hpp"

#include "monomux/client/Terminal.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("client/Terminal")

namespace monomux::client
{

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Terminal::Terminal(system::Handle::Raw InputStream,
                   system::Handle::Raw OutputStream)
  : AssociatedClient(nullptr)
{
#ifndef NDEBUG
  MovedFromCheck = true;
#endif

  using namespace monomux::system;

  std::ostringstream InName;
  std::ostringstream OutName;
  InName << "<terminal/input: " << InputStream << '>';
  OutName << "<terminal/output: " << OutputStream << '>';

  In = std::make_unique<unix::Pipe>(
    unix::Pipe::weakWrap(InputStream, Pipe::Read, InName.str()));
  Out = std::make_unique<unix::Pipe>(
    unix::Pipe::weakWrap(OutputStream, Pipe::Write, OutName.str()));
}

void Terminal::engage()
{
  if (engaged())
    throw std::logic_error{"Already engaged."};

  // TODO: Clear the screen and implement a redraw request from serverside.

  Engaged = true;
}

void Terminal::disengage()
{
  if (!engaged())
    return;

  // TODO: Clear the screen.

  Engaged = false;
}

void Terminal::clientInput(Terminal* Term, Client& Client)
{
  assert(Term->MovedFromCheck &&
         "Terminal object registered as callback was moved.");
  if (Client.getInputFile() != Term->input()->raw())
    throw std::invalid_argument{"Client InputFD != Terminal input"};

  do
  {
    static constexpr std::size_t ReadSize = BUFSIZ;
    std::string Input = Term->input()->read(ReadSize);
    if (Input.empty())
      return;

    Client.sendData(Input);
  } while (Term->input()->hasBufferedRead());
  Term->input()->tryFreeResources();
}

void Terminal::clientOutput(Terminal* Term, Client& Client)
{
  assert(Term->MovedFromCheck &&
         "Terminal object registered as callback was moved.");

  static constexpr std::size_t ReadSize = BUFSIZ;
  std::string Output = Client.getDataSocket()->read(ReadSize);
  Term->output()->write(Output);

  while (Term->output()->hasBufferedWrite())
    Term->output()->flushWrites();
  Term->output()->tryFreeResources();
}

void Terminal::clientEventReady(Terminal* Term, Client& Client)
{
  assert(Term->MovedFromCheck &&
         "Terminal object registered as callback was moved.");

  // if (Term->WindowSizeChanged.get().load())
  // {
  //   Size S = Term->getSize();
  //   Client.notifyWindowSize(S.Rows, S.Columns);
  //   Term->WindowSizeChanged.get().store(false);
  // }
}

void Terminal::setupClient(Client& Client)
{
  if (AssociatedClient)
    releaseClient();

  Client.setInputFile(In->raw());
  Client.setInputCallback(
    // NOLINTNEXTLINE(modernize-avoid-bind)
    std::bind(&Terminal::clientInput, this, std::placeholders::_1));
  Client.setDataCallback(
    // NOLINTNEXTLINE(modernize-avoid-bind)
    std::bind(&Terminal::clientOutput, this, std::placeholders::_1));
  Client.setExternalEventProcessor(
    // NOLINTNEXTLINE(modernize-avoid-bind)
    std::bind(&Terminal::clientEventReady, this, std::placeholders::_1));

  AssociatedClient = &Client;
}

void Terminal::releaseClient()
{
  if (!AssociatedClient)
    return;

  AssociatedClient->setDataCallback({});
  AssociatedClient->setInputCallback({});
  AssociatedClient->setExternalEventProcessor({});
  AssociatedClient->setInputFile(system::PlatformSpecificHandleTraits::Invalid);

  AssociatedClient = nullptr;
}

} // namespace monomux::client

#undef LOG
