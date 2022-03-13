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
#include "Client.hpp"

#include "control/Message.hpp"

#include <iostream>
#include <utility>

namespace monomux
{

std::optional<Client> Client::create(std::string SocketPath)
{
  try
  {
    Client Conn{Socket::open(SocketPath)};
    return Conn;
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "When creating ServerConnection with '" << SocketPath
              << "': " << Err.what() << std::endl;
  }
  return std::nullopt;
}

Client::Client(Socket&& ControlSock) : ControlSocket(std::move(ControlSock))
{
  setUpDispatch();
}

bool Client::handshake()
{
  // Authenticate the client on the server.
  {
    ControlSocket.write(encode(request::ClientID{}));
    std::string Data = ControlSocket.read(128);

    MessageBase MB = MessageBase::unpack(Data);
    if (MB.Kind != MessageKind::ClientIDResponse)
    {
      std::cerr
        << "ERROR: Invalid response from Server when trying to establish "
           "connection."
        << std::endl;
      return false;
    }
    responseClientID(MB.RawData);
    if (ClientID == static_cast<std::size_t>(-1) && !Nonce.has_value())
    {
      std::cerr
        << "ERROR: Invalid response from Server when trying to establish "
           "connection."
        << std::endl;
      return false;
    }
  }

  // If the control socket is now successfully established, establish another
  // connection to the same location, but for the data socket.
  auto DS = std::make_unique<Socket>(Socket::open(ControlSocket.getPath()));
  // (At this point, the server believes a brand new client has connected, and
  // is awaiting that client's handshake request. Instead, use our identity to
  // tell the server that this connection is in fact the same client, but it's
  // data connection.)
  {
    request::DataSocket Req;
    Req.Client.ID = ClientID;
    Req.Client.Nonce = consumeNonce();

    DS->write(encode(Req));
    std::string Data = DS->read(128);

    MessageBase MB = MessageBase::unpack(Data);
    std::cout << "DS result:" << MB.RawData << std::endl;
    if (MB.Kind != MessageKind::DataSocketResponse)
    {
      std::cerr << "ERROR: Invalid response from Server when trying to "
                   "establish Data connection."
                << std::endl;
      return false;
    }
    std::optional<response::DataSocket> Response =
      response::DataSocket::decode(MB.RawData);
    if (!Response.has_value())
    {
      std::cerr << "ERROR: Invalid response from Server when trying to "
                   "establish Data connection."
                << std::endl;
      return false;
    }
    if (!Response->Success)
    {
      std::cerr << "ERROR: Server rejected establishment of Data connection..."
                << std::endl;
      return false;
    }

    DataSocket = std::move(DS);

    // TODO: Request a new nonce here and store it, as the original one got
    // consumed.
  }

  return true;
}

std::size_t Client::consumeNonce() noexcept
{
  assert(Nonce.has_value());
  auto R = Nonce.value();
  Nonce.reset();
  return R;
}

void Client::requestSpawnProcess(const Process::SpawnOptions& Opts)
{
  // request::SpawnProcess Msg;
  // Msg.ProcessName = Opts.Program;
  // ControlSocket.write(encode(Msg));
}

void Client::sendData(std::string_view Data)
{
  if (!DataSocket)
  {
    std::cerr
      << "ERROR: Trying to send data but data connection was not established."
      << std::endl;
    return;
  }
  DataSocket->write(Data);
}

} // namespace monomux
