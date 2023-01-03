/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "monomux/adt/Atomic.hpp"
#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/ScopeGuard.hpp"
#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/Handle.hpp"
#include "monomux/system/IOEvent.hpp"
#include "monomux/system/Process.hpp"
#include "monomux/system/Socket.hpp"

#include "SessionData.hpp"

namespace monomux::client
{

/// This class represents a connection to a running \p Server - or rather,
/// a wrapper over the communication channel that allows talking to the
/// server. The client is responsible for reporting data sent from the server,
/// and can send user input to the server.
///
/// \note Some functionality of the client instance (e.g., sending signals over
/// to the attached session) requires proper signal handling, which the
/// \p Client does \b NOT implement internally! It is up to the program
/// embedding the client to construct and set up appropriate handlers!
class Client
{
public:
  /// Contains information about why the \p Client stopped its originally
  /// infinite job of handling I/O.
  struct Exit
  {
    enum Reason : std::uint8_t
    {
      None = 0,
      /// The client terminated because of internal logic failure. This is an
      /// \b error condition.
      Failed,
      /// The client was terminated by the user via a kill signal.
      Terminated,
      /// The client was terminated because the controlling terminal hung up.
      Hangup,
      /// The client exited because the server disconnected it.
      Detached,
      /// The client exited because the attached session exited.
      SessionExit,
      /// The client exit because the server shut down.
      ServerExit,
      /// The client was kicked by the server.
      ServerKicked,
    };

    /// A meaningful exit reason from an established set of values.
    Reason Reason = Reason::None;
    /// The raw exit code associated with the client-server connection
    /// exiting, if any.
    /// This field is not always meaningful!
    int SessionExitCode = 0;
    /// The message sent by the server when it decided to forcibly release the
    /// client, if any.
    /// This field is not always meaningful!
    std::string Message{};
  };

  /// The type of message handler functions.
  ///
  /// \param Client The \p Client manager that received the message.
  /// \param RawMessage A view into the buffer of the message, before any
  /// structural parsing had been applied.
  using HandlerFunction = void(Client& Client, std::string_view RawMessage);

  /// The type of a raw callback function that, when called, receives the
  /// \p Client instance associated with an event.
  using RawCallbackFn = void(Client& Client);

  /// Creates a new connection client to the server at the specified socket.
  ///
  /// \param RejectReason If specified, and the server gracefully rejected the
  /// connection, the reason for the rejection (as reported by the server) will
  /// be written to this variable.
  static std::optional<Client> create(std::string SocketPath,
                                      std::string* RejectReason);

  /// Initialise a \p Client over the already established \p ControlSocket.
  Client(std::unique_ptr<system::Socket>&& ControlSocket);

  /// Override the default handling logic for the specified message \p Kind to
  /// fire the user-given \p Handler \b instead \b of the built-in default.
  void registerMessageHandler(std::uint16_t Kind,
                              std::function<HandlerFunction> Handler);

  [[nodiscard]] const system::Socket& getControlSocket() const noexcept
  {
    return *ControlSocket;
  }
  MONOMUX_MEMBER_0(system::Socket&, getControlSocket, [[nodiscard]], noexcept);

  [[nodiscard]] const system::Socket* getDataSocket() const noexcept
  {
    return DataSocket ? DataSocket.get() : nullptr;
  }
  MONOMUX_MEMBER_0(system::Socket*, getDataSocket, [[nodiscard]], noexcept);

  /// Takes ownership of and stores the given \p Socket as the data socket of
  /// the client.
  ///
  /// \warning No appropriate handshaking is done by this call! The server
  /// needs to be communicated with in advance to associate the data connection
  /// with the already registered client!
  void setDataSocket(std::unique_ptr<system::Socket>&& DataSocket);

  [[nodiscard]] system::Handle::Raw getInputFile() const noexcept
  {
    return InputFile;
  }

  /// Sets the file which the client will consider its "input stream" and fire
  /// the \p InputCallback for.
  ///
  /// \param Handle A file descriptor to watch for, or the \e Invalid handle to
  /// disassociate any "input" from the client.
  void setInputFile(system::Handle::Raw Handle);

  /// Perform a handshake mechanism over the control socket.
  ///
  /// A successful handshake initialises the client to be fully \e capable of
  /// both control and data communication with the server, but does not start
  /// the handling logic (see \p loop()).
  ///
  /// \param FailureReason If set and the handshake procedure fails, a
  /// human-readable reason for it will be written.
  ///
  /// \return Whether the handshake process succeeded.
  bool handshake(std::string* FailureReason);

  /// Starts the main loop of the client. In the loop, the client will listen
  /// to messages arriving on either the \e Control, the \e Data connections or
  /// the \e Input file, and fire the registered handlers associated with these
  /// devices.
  void loop();

  [[nodiscard]] Exit getExitData() const noexcept { return ExitData; }

  /// Sends a request to the connected server to tell what sessions are running
  /// on the server.
  ///
  /// \returns The data received from the server, or \p nullopt, if
  /// commmuniation failed.
  [[nodiscard]] std::optional<std::vector<SessionData>> requestSessionList();

  /// Sends a request of new session creation to the server the client is
  /// connected to.
  ///
  /// \param Name The name to associate with the session. This is non-normative,
  /// and the server may overrule the request.
  /// \param Opts Details of the process to spawn on the server's end.
  ///
  /// \returns The actual name of the created session, if creation was
  /// successful.
  [[nodiscard]] std::optional<std::string>
  requestMakeSession(std::string Name, system::Process::SpawnOptions Opts);

  /// Sends a request to the server to attach the client to the session
  /// identified by \p SessionName.
  ///
  /// \return whether the attachment succeeded.
  [[nodiscard]] bool requestAttach(std::string SessionName);

  /// \returns whether the client successfully attached to a session on the
  /// server.
  ///
  /// \see requestAttach()
  [[nodiscard]] bool attached() const noexcept { return Attached; }

  /// \returns information about the session the client is (if \p attached() is
  /// \p true) or last was (if \p attached() is \p false) attached to. If the
  /// client never attached to any session, returns \p nullptr.
  [[nodiscard]] const SessionData* attachedSession() const noexcept
  {
    return AttachedSession ? &*AttachedSession : nullptr;
  }

  /// Sends \p Data to the server over the \e data connection.
  void sendData(std::string_view Data);

  /// Sends a request to the server to deliver \p Signal to the remote session's
  /// process.
  void sendSignal(int Signal);

  /// Sends a notification to the server that the dimensions of the window the
  /// client is running in has changed to the new \p Rows and \p Columns.
  void notifyWindowSize(unsigned short Rows, unsigned short Columns);


  /// Sets the handler that is fired when data is received from the server.
  /// The data is \b NOT read before the callback fires.
  void setDataCallback(std::function<RawCallbackFn> Callback);

  /// Sets the handler that is fired when data is received from the input of
  /// the client.
  /// The input is \b NOT read before the callback fires.
  void setInputCallback(std::function<RawCallbackFn> Callback);

  /// Sets the callback object for handling external events when the client's
  /// internal event handling \p loop() is ready for such.
  void setExternalEventProcessor(std::function<RawCallbackFn> Callback);

private:
  /// The control socket is used to communicate control commands with the
  /// server.
  std::unique_ptr<system::Socket> ControlSocket;

  /// The data connection is used to transmit the process data to the client.
  /// (This is initialised in a lazy fashion during operation.)
  std::unique_ptr<system::Socket> DataSocket;

  /// Whether continuous \e handling of data on the \p DataSocket
  /// (if connected) via \p Poll is enabled.
  UniqueScalar<bool, false> DataSocketEnabled;

  /// Whether the client successfully attached to a session on the server.
  UniqueScalar<bool, false> Attached;

  /// Information about the session the client attached to.
  std::optional<SessionData> AttachedSession;

  /// A callback that is fired when the client's event handling loop is
  /// "in the mood" for processing externalia, before \p loop() enters a
  /// \p wait() -like call on the operating system level.
  std::function<RawCallbackFn> ExternalEventProcessor;

  /// The callback fired when data becomes available on \p DataSocket.
  std::function<RawCallbackFn> DataHandler;
  /// The callback fired when data becomes available on \p InputFile.
  std::function<RawCallbackFn> InputHandler;

  /// Weak file handle for the stream that is considered the user-facing input
  /// device of the client.
  UniqueScalar<system::Handle::Raw,
               system::PlatformSpecificHandleTraits::Invalid>
    InputFile;

  /// Whether continuous \e handling of inputs on the \p InputFile (if set) via
  /// \p Poll is enabled.
  UniqueScalar<bool, false> InputFileEnabled;

  Exit ExitData;
  /// Terminate the handling \p loop() of the client and set the exit status to
  /// \p E, the process-level exit code to \p ECode, and the exit message to
  /// \p Message.
  void exit(enum Exit::Reason E, int ECode, std::string Message);

  mutable Atomic<bool> TerminateLoop = false;
  std::unique_ptr<system::IOEvent> Poll;

  /// A unique identifier of the current \p Client, as returned by the server.
  std::size_t ClientID = -1;

  /// A unique, random generated single-use number, which the \p Client can
  /// use to establish its identity towards the server in another request.
  std::optional<std::size_t> Nonce;

  /// Return the stored \p Nonce of the current instance, resetting it.
  std::size_t consumeNonce() noexcept;

  /// Maps \p MessageKind to handler functions.
  std::map<std::uint16_t, std::function<HandlerFunction>> Dispatch;

  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  static void FUNCTION_NAME(Client& Client, std::string_view Message);
#include "Dispatch.ipp"

  /// The callback that is fired when data is available on the \e control
  /// connection of the client. This method deals with parsing a \p Message
  /// from the control connection, and fire a message-specific handler.
  ///
  /// \see registerMessageHandler().
  void controlCallback();

  /// A trivial scope guard.
  using Inhibitor = ScopeGuard<std::function<void()>, std::function<void()>>;

public:
  /// If channel polling is initialised, adds \p ControlSocket to the list of
  /// channels to poll and handle incoming messages from.
  void enableControlResponse();
  /// If channel polling is initialised, removes \p ControlSocket from the list
  /// of channels to poll. When inhibited, messages sent by the server are
  /// expected to be handled synchronously by the function that did something
  /// resulting in the server sending responses (usually a request sending
  /// function) instead of being handled "automatically" by a dispatch handler.
  void disableControlResponse();
  /// A scope-guard version that calls \p disableControlResponse() and
  /// \p enableControlResponse() when entering and leaving scope.
  [[nodiscard]] Inhibitor inhibitControlResponse();

  /// If channel polling is initialised, adds \p DataSocket to the list of
  /// channels to poll and handle incoming data from.
  void enableDataSocket();
  /// If channel polling is initialised, removes \p DataSocket from the list of
  /// channels to poll. When disabled, data sent by the server is left
  /// unhandled.
  void disableDataSocket();
  /// A scope-guard version that calls \p disableDataSocket() and
  /// \p enableDataSocket() when entering and leaving scope.
  [[nodiscard]] Inhibitor inhibitDataSocket();

  /// If channel polling is initialised, adds the input device to the list of
  /// channels to poll and handle incoming data from.
  void enableInputFile();
  /// If channel polling is initialised, removes the input device from the list
  /// of channels to poll. When inhibited, input that appears on the input
  /// device will not trigger the \p InputCallback.
  void disableInputFile();
  /// A scope-guard version that calls \p disableInputFile() and
  /// \p enableInputFile() when entering and leaving scope.
  [[nodiscard]] Inhibitor inhibitInputFile();
};

} // namespace monomux::client
