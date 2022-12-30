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
#include <chrono>
#include <iomanip>
#include <set>
#include <thread>

#include "monomux/CheckedErrno.hpp"
#include "monomux/Config.h"
#include "monomux/Time.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/message/PascalString.hpp"
#include "monomux/system/BufferedChannel.hpp"
#include "monomux/system/Handle.hpp"
#include "monomux/system/IOEvent.hpp"
#include "monomux/system/Process.hpp"

#ifdef MONOMUX_PLATFORM_UNIX
#include "monomux/system/EPoll.hpp"
#include "monomux/system/fd.hpp"
#endif /* MONOMUX_PLATFORM_UNIX */

#include "monomux/server/Server.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("server/Server")

namespace monomux::server
{

Server::Server(std::unique_ptr<system::Socket>&& Sock)
  : Sock(std::move(Sock)), ExitIfNoMoreSessions(false)
{
  setUpMainDispatch();
  DeadChildren.fill(system::PlatformSpecificProcessTraits::Invalid);
}

Server::~Server() = default;

#if MONOMUX_EMBEDDING_LIBRARY_FEATURES
void Server::registerPreMessageHandler(std::uint16_t Kind,
                                       std::function<HandlerFunction> Handler)
{
  PreDispatch[Kind] = std::move(Handler);
}

void Server::registerPostMessageHandler(std::uint16_t Kind,
                                        std::function<HandlerFunction> Handler)
{
  PostDispatch[Kind] = std::move(Handler);
}
#endif /* MONOMUX_EMBEDDING_LIBRARY_FEATURES */

void Server::setExitIfNoMoreSessions(bool ExitIfNoMoreSessions)
{
  this->ExitIfNoMoreSessions = ExitIfNoMoreSessions;
}

/// Reschedules the overflown buffer identified by \p BO to the next iteration
/// of \p Poll.
static void rescheduleOverflow(system::IOEvent& Poll,
                               const system::BufferedChannel::OverflowError& BO)
{
  Poll.schedule(BO.fd(), BO.readOverflow(), BO.writeOverflow());
}

/// Tries to flush the contents of the socket, and if the flushing fails,
/// schedules it for the next iteration of \p Poll.
static void flushAndReschedule(system::IOEvent& Poll, system::Socket& S)
{
  S.flushWrites();
  if (S.hasBufferedWrite())
    Poll.schedule(S.raw(), /* Incoming =*/false, /* Outgoing =*/true);
}

void Server::loop()
{
  using namespace monomux::system;

  static constexpr std::size_t ListenQueue = 16;
  static constexpr std::size_t EventQueue = 1 << 13;

  WhenStarted = std::chrono::system_clock::now();

#ifdef MONOMUX_PLATFORM_UNIX
  unix::fd::addStatusFlag(Sock->raw(), O_NONBLOCK);
  Poll = std::make_unique<unix::EPoll>(EventQueue);
#endif /* MONOMUX_PLATFORM_UNIX */

  if (!Poll)
  {
    LOG(fatal) << "No I/O Event poll was created, but this is a critical "
                  "needed functionality.";
    return;
  }
  Poll->listen(Sock->raw(), /* Incoming =*/true, /* Outgoing =*/false);

  Sock->listen(ListenQueue);

  auto NewClient = [this]() -> bool {
    std::error_code Error;
    bool Recoverable;
    std::unique_ptr<system::Socket> ClientSock =
      Sock->accept(&Error, &Recoverable);
    if (!ClientSock)
    {
      if (Recoverable)
        LOG(warn) << "accept() did not succeed: " << Error;
      else
        LOG(error) << "accept() did not succeed: " << Error
                   << " (not recoverable)";

      if (Recoverable)
      {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
      }
      return false;
    }

    // A new client was accepted.
    if (ClientData* ExistingClient = getClient(ClientSock->raw()))
    {
      // The client with the same socket FD is already known.
      // TODO: What is the good way of handling this?
      LOG(debug) << "Stale socket of gone client, " << ClientSock->raw()
                 << " left behind?";
      clientExit(*ExistingClient);
      removeClient(*ExistingClient);
    }

    ClientData* Client = makeClient(ClientData{std::move(ClientSock)});
    clientCreate(*Client);
    return false;
  };

  while (!TerminateLoop.get().load())
  {
    // Process "external" events.
    reapDeadChildren();

    const std::size_t NumTriggeredFDs = Poll->wait();
    MONOMUX_TRACE_LOG(LOG(data) << NumTriggeredFDs << " events received!");
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      IOEvent::EventWithMode Event;
      try
      {
        Event = Poll->eventAt(I);
      }
      catch (...)
      {
        Event.FD = PlatformSpecificHandleTraits::Invalid;
      }
      if (!Handle::isValid(Event.FD))
      {
        LOG(error) << '#' << I
                   << " event received but there was no associated file";
        continue;
      }

      if (Event.FD == Sock->raw())
      {
        // Event occured on the main socket.
        bool Retry = NewClient();
        if (Retry)
          --I;
        continue;
      }

      // Event occured on another (connected client or session) socket.
      MONOMUX_TRACE_LOG(LOG(trace)
                        << "Event on file descriptor " << Event.FD
                        << " (incoming: " << std::boolalpha << Event.Incoming
                        << ", outgoing: " << Event.Outgoing << std::noboolalpha
                        << ')');

      LookupVariant* Entity = FDLookup.tryGet(Event.FD);
      if (!Entity)
      {
        LOG(error) << "\tEntity for file descriptor " << Event.FD
                   << " missing from lookup table? (Possible internal error, "
                      "race condition, or mid-handling disconnect?)";
        continue;
      }
      try
      {
        if (auto* Session = std::get_if<SessionConnection>(Entity))
        {
          SessionData& S = **Session;
          if (Event.Incoming)
          {
            // First check for data coming from a session. This is the most
            // populous in terms of bandwidth.
            dataCallback(S);
            S.getReader()->tryFreeResources();
          }
          if (Event.Outgoing)
          {
            try
            {
              S.getWriter()->flushWrites();
              S.getWriter()->tryFreeResources();
            }
            catch (const BufferedChannel::OverflowError& BO)
            {
              rescheduleOverflow(*Poll, BO);
            }
          }
          continue;
        }
        if (auto* Data = std::get_if<ClientDataConnection>(Entity))
        {
          ClientData& C = **Data;
          auto ClientID = C.id();

          if (Event.Incoming)
            // Second, try to see if the data is coming from a client, like
            // keypresses and such. We expect to see many of these, too.
            dispatchData(C);
          if (Event.Outgoing)
            flushAndReschedule(*Poll, *C.getDataSocket());

          if (Clients.find(ClientID) != Clients.end())
            C.getDataSocket()->tryFreeResources();
          continue;
        }
        if (auto* Control = std::get_if<ClientControlConnection>(Entity))
        {
          ClientData& C = **Control;
          auto ClientID = C.id();

          if (Event.Incoming)
            // Lastly, check if the receive is happening on the control
            // connection, where messages are small and far inbetween.
            dispatchControl(C);
          if (Event.Outgoing)
            flushAndReschedule(*Poll, C.getControlSocket());

          if (Clients.find(ClientID) != Clients.end())
            C.getControlSocket().tryFreeResources();
          continue;
        }
      }
      catch (const BufferedChannel::OverflowError& BO)
      {
        LOG(error) << "Generic handling error:\n\t" << BO.what();
        rescheduleOverflow(*Poll, BO);
      }
      catch (const std::system_error& Err)
      {
        // Ignore the error on the sockets and pipes, and do not tear the
        // server down just because of them.
        LOG(error) << "Generic handling error:\n\t" << Err.what();
      }
    }
  }
}

void Server::interrupt() const noexcept { TerminateLoop.get().store(true); }

static void sendKickClient(ClientData& Client, std::string Reason)
{
  try
  {
    Client.sendDetachReason(
      monomux::message::notification::Detached::Kicked, 0, std::move(Reason));
  }
  // Ignore the error. It could be that the client got auto-detached when
  // their associated session ended.
  catch (const system::BufferedChannel::OverflowError&)
  {}
  catch (const std::system_error&)
  {}
}

void Server::shutdown()
{
  LOG(info) << "Detaching all clients...";
  while (!Clients.empty())
  {
    ClientData& Client = *Clients.begin()->second;
    try
    {
      Client.sendDetachReason(
        monomux::message::notification::Detached::ServerShutdown);
    }
    // Ignore the error. It could be that the client got auto-detached when
    // their associated session ended.
    catch (const system::BufferedChannel::OverflowError&)
    {}
    catch (const std::system_error&)
    {}
    removeClient(Client);
  }

  LOG(info) << "Terminating all sessions...";
  while (!Sessions.empty())
  {
    SessionData& Session = *Sessions.begin()->second;
    removeSession(Session);
  }
}

ClientData* Server::getClient(std::size_t ID) noexcept
{
  auto It = Clients.find(ID);
  return It != Clients.end() ? It->second.get() : nullptr;
}

SessionData* Server::getSession(std::string_view Name) noexcept
{
  auto It =
    std::find_if(Sessions.begin(), Sessions.end(), [Name](const auto& Elem) {
      return Elem.first == Name;
    });
  return It != Sessions.end() ? It->second.get() : nullptr;
}

ClientData* Server::makeClient(ClientData Client)
{
  std::size_t CID = Client.id();
  auto InsertRes =
    Clients.try_emplace(CID, std::make_unique<ClientData>(std::move(Client)));
  if (!InsertRes.second)
    return nullptr;
  return InsertRes.first->second.get();
}

SessionData* Server::makeSession(SessionData Session)
{
  std::string SN = Session.name();
  auto InsertRes =
    Sessions.try_emplace(SN, std::make_unique<SessionData>(std::move(Session)));
  if (!InsertRes.second)
    return nullptr;
  return InsertRes.first->second.get();
}

void Server::removeClient(ClientData& Client)
{
  std::size_t CID = Client.id();
  if (SessionData* S = Client.getAttachedSession())
    clientDetached(Client, *S);
  Clients.erase(CID);
}

void Server::removeSession(SessionData& Session)
{
  for (ClientData* C : Session.getAttachedClients())
    clientDetached(*C, Session);

  Sessions.erase(Session.name());

  if (Sessions.empty() && ExitIfNoMoreSessions)
    TerminateLoop.get().store(true);
}

void Server::registerDeadChild(system::Process::Raw PID) const noexcept
{
  for (std::size_t I = 0; I < DeadChildrenVecSize; ++I)
    if (DeadChildren.at(I) == system::PlatformSpecificProcessTraits::Invalid)
    {
      DeadChildren.at(I) = PID;
      return;
    }
}

void Server::clientCreate(ClientData& Client)
{
  LOG(info) << "Client \"" << Client.id() << "\" connected";
  system::Handle::Raw FD = Client.getControlSocket().raw();

  // (8 is a good guesstimate because FDLookup usually counts from 5 or 6, not
  // from 0.)
  static constexpr std::size_t FDKeepSpare = 8;
  std::size_t FDCount = FDLookup.size();
  std::size_t MaxFDs = system::Handle::maxHandles() - FDKeepSpare;
  if (FDCount >= MaxFDs)
  {
    // As a full client connection would require *TWO* file descriptors (control
    // and data socket) and we would need to keep 1 open so we can always
    // accept() a connection, reject the client if there aren't any space left.
    LOG(warn) << "Self-defence rejecting client - " << FDCount
              << " FDs allocated out of the max " << MaxFDs;
    sendRejectClient(Client, "Not enough file descriptors left on server.");
    removeClient(Client);
    return;
  }

#ifdef MONOMUX_PLATFORM_UNIX
  system::unix::fd::setNonBlockingCloseOnExec(FD);
#endif /* MONOMUX_PLATFORM_UNIX */
  Poll->listen(FD, /* Incoming =*/true, /* Outgoing =*/false);
  FDLookup[FD] = ClientControlConnection{&Client};

  sendAcceptClient(Client);
}

void Server::dispatchControl(ClientData& Client)
{
  using namespace monomux::message;
  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Client \"" << Client.id() << "\" sent CONTROL!");
  system::Socket& ClientSock = Client.getControlSocket();
  std::string Data;

  try
  {
    Data = readPascalString(ClientSock);
  }
  catch (const system::BufferedChannel::OverflowError& BO)
  {
    LOG(trace) << "Client \"" << Client.id()
               << "\": error when reading CONTROL: "
               << "\n\t" << BO.what();
    rescheduleOverflow(*Poll, BO);
    return;
  }
  catch (const std::system_error& Err)
  {
    LOG(error) << "Client \"" << Client.id()
               << "\": error when reading CONTROL: " << Err.what();
  }

  if (ClientSock.failed())
  {
    // We realise the client disconnected during an attempt to read.
    clientExit(Client);
    return;
  }

  if (ClientSock.hasBufferedRead())
    Poll->schedule(ClientSock.raw(), /* Incoming =*/true, /* Outgoing =*/false);
  else
    ClientSock.tryFreeResources();

  if (Data.empty())
    return;

  Message MB = Message::unpack(Data);
  MONOMUX_TRACE_LOG(LOG(data) << "Client \"" << Client.id() << "\"\n"
                              << MB.RawData);

  using Kind = decltype(MainDispatch)::key_type;

#if MONOMUX_EMBEDDING_LIBRARY_FEATURES
  {
    auto Action = PreDispatch.find(static_cast<Kind>(MB.Kind));
    if (Action != PreDispatch.end())
    {
      try
      {
        Action->second(*this, Client, MB.RawData);
      }
      catch (const system::BufferedChannel::OverflowError& BO)
      {
        LOG(trace) << "Client " << '"' << Client.id() << '"' << ':'
                   << " error when pre-handling message"
                   << "\n\t" << BO.what();
        rescheduleOverflow(*Poll, BO);
      }
      catch (const std::system_error& Err)
      {
        LOG(error) << "Client " << '"' << Client.id() << '"' << ':'
                   << " error when pre-handling message";
        if (ClientSock.failed())
          clientExit(Client);
      }
      catch (const HandlingPreventingException&)
      {
        LOG(trace) << "Client " << '"' << Client.id() << '"' << ':'
                   << " pre-handler inhibited main handler";
        return;
      }
    }
  }
#endif /* MONOMUX_EMBEDDING_LIBRARY_FEATURES */

  auto Action = MainDispatch.find(static_cast<Kind>(MB.Kind));
  if (Action == MainDispatch.end())
  {
    MONOMUX_TRACE_LOG(LOG(trace) << "Client " << '"' << Client.id() << '"'
                                 << ':' << " unknown message type "
                                 << static_cast<int>(MB.Kind) << " received");
  }
  else
  {
    try
    {
      Action->second(*this, Client, MB.RawData);
    }
    catch (const system::BufferedChannel::OverflowError& BO)
    {
      LOG(trace) << "Client " << '"' << Client.id() << '"' << ':'
                 << " error when handling message"
                 << "\n\t" << BO.what();
      rescheduleOverflow(*Poll, BO);
    }
    catch (const std::system_error& Err)
    {
      LOG(error) << "Client " << '"' << Client.id() << '"' << ':'
                 << " error when handling message";
      if (ClientSock.failed())
        clientExit(Client);
    }
  }

#if MONOMUX_EMBEDDING_LIBRARY_FEATURES
  {
    auto Action = PostDispatch.find(static_cast<Kind>(MB.Kind));
    if (Action != PostDispatch.end())
    {
      try
      {
        Action->second(*this, Client, MB.RawData);
      }
      catch (const system::BufferedChannel::OverflowError& BO)
      {
        LOG(trace) << "Client " << '"' << Client.id() << '"' << ':'
                   << " error when post-handling message"
                   << "\n\t" << BO.what();
        rescheduleOverflow(*Poll, BO);
      }
      catch (const std::system_error& Err)
      {
        LOG(error) << "Client " << '"' << Client.id() << '"' << ':'
                   << " error when post-handling message";
        if (ClientSock.failed())
          clientExit(Client);
      }
    }
  }
#endif /* MONOMUX_EMBEDDING_LIBRARY_FEATURES */
}

void Server::dispatchData(ClientData& Client)
{

  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Client \"" << Client.id() << "\" sent DATA!");
  system::Socket& DS = *Client.getDataSocket();
  std::string Data;
  try
  {
    Data = DS.read(DS.optimalReadSize());
  }
  catch (const system::BufferedChannel::OverflowError& BO)
  {
    LOG(error) << "Client \"" << Client.id() << "\": error when reading DATA: "
               << "\n\t" << BO.what();
    sendKickClient(Client,
                   "Overflow when reading connection, " +
                     std::to_string(BO.channel().readInBuffer()) +
                     " bytes already pending");
    clientExit(Client);
    return;
  }
  catch (const std::system_error& Err)
  {
    LOG(error) << "Client \"" << Client.id()
               << "\": error when reading DATA: " << Err.what();
    return;
  }

  if (DS.failed())
  {
    // We realise the client disconnected during an attempt to read.
    clientExit(Client);
    return;
  }

  if (DS.hasBufferedRead())
    Poll->schedule(DS.raw(), /* Incoming =*/true, /* Outgoing =*/false);

  Client.activity();
  MONOMUX_TRACE_LOG(LOG(data)
                    << "Client \"" << Client.id() << "\" data: " << Data);

  if (SessionData* S = Client.getAttachedSession())
    try
    {
      S->getWriter()->write(Data);
    }
    catch (const system::BufferedChannel::OverflowError& BO)
    {
      LOG(trace) << "Session \"" << S->name()
                 << "\" when relaying input from client \"" << Client.id()
                 << "\"\n\t" << BO.what();
      rescheduleOverflow(*Poll, BO);
    }
}

void Server::clientExit(ClientData& Client)
{
  LOG(info) << "Client \"" << Client.id() << "\" exited";

  if (const auto* DS = Client.getDataSocket())
  {
    Poll->stop(DS->raw());
    FDLookup.erase(DS->raw());
  }

  Poll->stop(Client.getControlSocket().raw());
  FDLookup.erase(Client.getControlSocket().raw());

  removeClient(Client);
}

void Server::sessionCreate(SessionData& Session)
{
  LOG(info) << "Session \"" << Session.name() << "\" created";
  if (Session.hasProcess() && Session.getProcess().hasPty())
  {
    system::Handle::Raw FD = Session.getIdentifyingHandle();

    Poll->listen(FD, /* Incoming =*/true, /* Outgoing =*/false);
    FDLookup[FD] = SessionConnection{&Session};
  }
}

void Server::dataCallback(SessionData& Session)
{
  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Session \"" << Session.name() << "\" sent DATA!");
  std::string Data;
  try
  {
    Data = Session.getReader()->read(Session.getReader()->optimalReadSize());
  }
  catch (const system::BufferedChannel::OverflowError& BO)
  {
    LOG(error) << "Session \"" << Session.name()
               << "\": error when reading DATA: "
               << "\n\t" << BO.what();
    rescheduleOverflow(*Poll, BO);
    return;
  }
  catch (const std::system_error& Err)
  {
    LOG(error) << "Session \"" << Session.name()
               << "\": error when reading DATA: " << Err.what();
    return;
  }

  if (Session.getReader()->hasBufferedRead())
    Poll->schedule(Session.getIdentifyingHandle(),
                   /* Incoming =*/true,
                   /* Outgoing =*/false);

  Session.activity();
  MONOMUX_TRACE_LOG(LOG(data)
                    << "Session \"" << Session.name() << "\" data: " << Data);

  for (ClientData* C : Session.getAttachedClients())
    if (system::Socket* DS = C->getDataSocket())
    {
      try
      {
        DS->write(Data);
      }
      catch (const system::BufferedChannel::OverflowError& BO)
      {
        // This is the part that can usually hang if there is too much data
        // coming from the session that can't be sent to the clients in a
        // timely manner.
        sendKickClient(*C,
                       "Overflow when sending, " +
                         std::to_string(BO.channel().writeInBuffer()) +
                         " bytes already pending");
        clientExit(*C);
        continue;
      }
      catch (const std::system_error& Err)
      {
        LOG(error) << "Session \"" << Session.name()
                   << "\": error when sending DATA to attached client \""
                   << C->id() << "\": " << Err.what();

        if (DS->failed())
        {
          // We realise the client disconnected during an attempt to send.
          clientExit(*C);
          continue;
        }
      }

      if (DS->hasBufferedWrite())
        Poll->schedule(DS->raw(), /* Incoming =*/false, /* Outgoing =*/true);
    }
}

void Server::clientAttached(ClientData& Client, SessionData& Session)
{
  LOG(info) << "Client \"" << Client.id() << "\" attached to \""
            << Session.name() << '"';
  Client.attachToSession(Session);
  Session.attachClient(Client);
}

void Server::clientDetached(ClientData& Client, SessionData& Session)
{
  if (Client.getAttachedSession() != &Session)
    return;
  LOG(info) << "Client \"" << Client.id() << "\" detached from \""
            << Session.name() << '"';
  Client.detachSession();
  Session.removeClient(Client);
}

void Server::sessionDestroy(SessionData& Session)
{
  LOG(info) << "Session \"" << Session.name() << "\" exited";
  if (Session.hasProcess() && Session.getProcess().hasPty())
  {
    system::Handle::Raw FD = Session.getProcess().getPty()->raw();

    Poll->stop(FD);
    FDLookup.erase(FD);
  }

  removeSession(Session);
}

void Server::turnClientIntoDataOfOtherClient(ClientData& MainClient,
                                             ClientData& DataClient)
{
  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Client \"" << DataClient.id()
                    << "\" becoming the DATA connection for Client \""
                    << MainClient.id() << '"');
  MainClient.subjugateIntoDataSocket(DataClient);
  FDLookup[MainClient.getDataSocket()->raw()] =
    ClientDataConnection{&MainClient};

  // Remove the object from the owning data structure but do not fire the exit
  // handler!
  Clients.erase(DataClient.id());
}

void Server::reapDeadChildren()
{
  for (system::Process::Raw& PID : DeadChildren)
  {
    if (PID == system::PlatformSpecificProcessTraits::Invalid)
      continue;

    auto SessionForProc =
      std::find_if(Sessions.begin(), Sessions.end(), [PID](auto&& E) {
        return E.second->hasProcess() && E.second->getProcess().raw() == PID;
      });
    if (SessionForProc == Sessions.end())
      continue;
    system::Process& Proc = SessionForProc->second->getProcess();

    bool Dead = Proc.reapIfDead();
    if (Dead)
    {
      LOG(debug) << "Child PID " << PID << " of Session \""
                 << SessionForProc->second->name() << "\" exited with "
                 << Proc.exitCode();

      for (ClientData* AC : SessionForProc->second->getAttachedClients())
        AC->sendDetachReason(monomux::message::notification::Detached::Exit,
                             Proc.exitCode());
      sessionDestroy(*SessionForProc->second);
    }

    PID = system::PlatformSpecificProcessTraits::Invalid;
  }
}

std::string Server::statistics() const
{
  std::ostringstream Output;
  std::size_t IndentSize = 0;
  const auto Indented = [&Output, &IndentSize]() -> std::ostringstream& {
    Output << std::string(IndentSize, ' ');
    return Output;
  };
  const auto Reindent = [&Indented](const std::string& S) {
    std::string::size_type Pos1 = 0;
    std::string::size_type Pos2 = S.find('\n');

    while (Pos1 < S.size() && Pos2 < S.size() && Pos2 != std::string::npos)
    {
      Indented() << S.substr(Pos1, Pos2 - Pos1) << '\n';
      Pos1 = Pos2 + 1;
      Pos2 = S.find('\n', Pos1);
    }
  };
  const auto AddIndent = [&IndentSize](const std::size_t Value) -> std::string {
    IndentSize += Value;
    return "";
  };
  const auto ResetIndent = [&IndentSize] { IndentSize = 0; };
  /// Save the indentation state at the moment of constructing this instance,
  /// and restore it when it is destroyed.
  struct IndentRAII
  {
    std::size_t& OutVariable;
    std::size_t SavedIndent;

    IndentRAII(std::size_t& Indent) : OutVariable(Indent), SavedIndent(Indent)
    {}
    ~IndentRAII() { OutVariable = SavedIndent; }
  };
  const auto IndentScope = [&IndentSize]() -> IndentRAII {
    return {IndentSize};
  };

  const auto DumpOneClient =
    [&Output, &AddIndent, &Indented, &Reindent, &IndentScope](
      const ClientData& C) {
      auto X = IndentScope();
      Output << "Client " << '\'' << C.id() << '\'' << '\n';
      AddIndent(2);
      Indented() << "* Connected         : " << formatTime(C.whenCreated())
                 << '\n';
      Indented() << "* LastActive        : " << formatTime(C.lastActive())
                 << '\n';

      auto& Cl = const_cast<ClientData&>(C);
      Indented() << "* Control Connection:" << '\n';
      {
        auto X = IndentScope();
        AddIndent(4);
        Reindent(Cl.getControlSocket().statistics());
      }

      if (auto* DS = Cl.getDataSocket())
      {
        Indented() << "* Data    Connection:" << '\n';

        auto X = IndentScope();
        AddIndent(4);
        Reindent(DS->statistics());
      }
    };

  Output << "MonoMux Server Statistics\n";

  AddIndent(2);
  Indented() << "on " << '\'' << Sock->identifier() << '\'' << '\n';
  Indented() << "started at " << formatTime(whenStarted()) << '\n';
  AddIndent(2);
  Output << '\n';
  Indented() << "* Attached clients               : " << Clients.size() << '\n';
  Indented() << "* Running sessions               : " << Sessions.size()
             << '\n';
  Indented() << "* Open file descriptors in total : " << FDLookup.size()
             << '\n';

  std::set<std::size_t> AlreadyDumpedAttachedClients;
  Output << '\n'
         << "- = - = - = - = -"
         << "         Sessions        "
         << "- = - = - = - = -" << '\n';
  ResetIndent();
  AddIndent(2);
  for (const auto& E : Sessions)
  {
    const SessionData& S = *E.second;
    auto X = IndentScope();

    Indented() << "# Session " << '\'' << S.name() << '\'' << '\n';
    AddIndent(2);
    Indented() << "* Created     : " << formatTime(S.whenCreated()) << '\n';
    Indented() << "* LastActive  : " << formatTime(S.lastActive()) << '\n';

    if (S.hasProcess())
    {
      auto& P = const_cast<system::Process&>(S.getProcess());
      Indented() << "* Running PID : " << P.raw() << '\n';
      if (P.hasPty())
      {
        Indented() << "* Communication "
                   << "reader" << '\n';
        {
          auto X = IndentScope();
          AddIndent(4);
          Reindent(P.getPty()->reader().statistics());
        }
        Indented() << "* Communication "
                   << "writer" << '\n';
        {
          auto X = IndentScope();
          AddIndent(4);
          Reindent(P.getPty()->writer().statistics());
        }
      }
      else
        Indented() << "! Associated Process does not have a PTY\n";
    }
    else
      Indented() << "! No process associated with Session\n";

    Indented() << "* Attached client #: " << S.getAttachedClients().size()
               << '\n';
    AddIndent(4);
    for (const ClientData* C : S.getAttachedClients())
    {
      Indented() << '*' << ' ';
      DumpOneClient(*C);
      AlreadyDumpedAttachedClients.emplace(C->id());
    }
  }

  Output << '\n'
         << "- = - = - = - = -"
         << "   Unassociated Clients   "
         << "- = - = - = - = -" << '\n';
  ResetIndent();
  AddIndent(2);
  for (const auto& E : Clients)
  {
    const ClientData& C = *E.second;
    if (AlreadyDumpedAttachedClients.find(C.id()) !=
        AlreadyDumpedAttachedClients.end())
      continue;

    Indented() << '#' << ' ';
    DumpOneClient(C);
  }

  return Output.str();
}

} // namespace monomux::server

#undef LOG
