//
// SSHShellConnection.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  SSHShellConnection
//
// OSP shell session handler over SSH.
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//


#ifndef OSP_Shell_SSHShellConnection_INCLUDED
#define OSP_Shell_SSHShellConnection_INCLUDED


#include "Poco/SSH/SSHSession.h"
#include "Poco/SSH/SSHServer.h"
#include "Poco/SSH/SSHServerConfig.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Event.h"
#include "Poco/Mutex.h"
#include "Poco/Thread.h"
#include <chrono>
#include <memory>
#include <string>


namespace Poco {
namespace OSP {
namespace Shell {


class SessionTracker;


class SSHShellConnection: public Poco::SSH::SSHSession
	/// OSP shell session handler over SSH.
	///
	/// Extends SSHSession to run the OSP command loop
	/// over an SSH channel after authentication.
	///
	/// Rejected password logins and rejected logins with the login
	/// command are handled as in ShellConnection: logged with the client
	/// address, answered after the delay of the address, and the connection
	/// is closed after ShellConnection::MAX_FAILED_LOGINS of them. A session
	/// in which no command arrives within the session timeout is closed.
	///
	/// A connection on which no shell session starts within the login grace
	/// time is closed.
	///
	/// The commands of a session after a password login are authorized under
	/// the account name that the authentication service has returned for the
	/// login. A public key login is not checked by the authentication service,
	/// so the commands of its session are authorized under the SSH user name.
{
public:
	SSHShellConnection(
		ssh_session session,
		const Poco::SSH::SSHServerConfig& config,
		Poco::SSH::SSHServer& server,
		const ShellConnection::Config& shellConfig,
		SessionTracker& sessions,
		std::chrono::seconds loginGraceTime);
		/// Creates the SSHShellConnection. The objects that shellConfig refers
		/// to and the SessionTracker must exist as long as the connection.
		///
		/// A login grace time of zero disables the limit.

	~SSHShellConnection();

	[[nodiscard]] static bool stopServer(Poco::SSH::SSHServer& server, SessionTracker& sessions, Poco::Thread& stopper,
		std::chrono::milliseconds timeout);
		/// Ends the delays after rejected logins in the given sessions and
		/// stops the server in the given thread, which closes the port of the
		/// server and the connections of the sessions. Waits until the server
		/// has stopped, at most for the given time, and returns true if it has.
		///
		/// Returns false if a session has not ended within this time, for
		/// example because it runs a command. The thread then stops the server
		/// when the sessions have ended; until then, the server, the thread,
		/// the SessionTracker and the objects that the shell config of the
		/// sessions refers to are still in use.

protected:
	void handleSession(ssh_channel channel, const std::string& sshUserName) override;

private:
	SSHShellConnection(
		ssh_session session,
		const Poco::SSH::SSHServerConfig& config,
		Poco::SSH::SSHServer& server,
		const ShellConnection::Config& shellConfig,
		SessionTracker& sessions,
		std::chrono::seconds loginGraceTime,
		const std::shared_ptr<std::string>& pAccountName);
		/// Creates the SSHShellConnection, whose password authenticator stores
		/// the account name of an accepted login in pAccountName.

	enum class ReadResult
	{
		LINE,
		END,
		TIMEOUT,
		TOO_LONG
	};

	[[nodiscard]] ReadResult readLine(ssh_channel channel, std::string& line) const;
		/// Reads a command line from the channel and echoes it. The whole line
		/// must arrive within the session timeout.
		///
		/// Control characters other than the line ends, backspace and Ctrl-C
		/// are dropped. Bytes of 0x80 and above, as in UTF-8 text, are kept, as
		/// on the TCP transport.

	[[nodiscard]] static Poco::Net::SocketAddress clientAddressOf(ssh_session session);
		/// Returns the address of the client, or the wildcard address if the
		/// system does not report it.

	[[nodiscard]] static Poco::SSH::SSHServerConfig withLoginChecks(
		const Poco::SSH::SSHServerConfig& config,
		const Poco::Net::SocketAddress& clientAddress,
		const ShellConnection::Config& shellConfig,
		SessionTracker& sessions,
		const std::shared_ptr<std::string>& pAccountName);
		/// Returns the config with a password authenticator that checks the
		/// password with the authentication service, stores the account name
		/// of an accepted login in pAccountName and handles a rejection.

	void closeWithoutLogin(std::chrono::seconds loginGraceTime);
		/// Shuts down the connection, unless the shell session starts or the
		/// connection is destroyed within the login grace time.

	ShellConnection::Config _shellConfig;
	SessionTracker& _sessions;
	std::shared_ptr<std::string> _pAccountName;
		/// The account name of the password login of the connection, which
		/// the password authenticator and handleSession() use on the thread
		/// of the connection. Empty after a public key login.
	Poco::Net::SocketAddress _clientAddress;
	Poco::Net::StreamSocket _socket;
		/// A second descriptor of the connection. libssh closes its own
		/// descriptor before the connection is destroyed; this one keeps the
		/// socket open until then, so that the login timer never shuts down
		/// a descriptor that the system has given to another file.
	Poco::FastMutex _sessionMutex;
	bool _sessionStarted{false};
		/// Set by handleSession() when the shell session starts. The login
		/// timer shuts down the connection only if it is not set, and holds
		/// _sessionMutex while it checks it and shuts down, so that it never
		/// shuts down a shell session that has started.
	Poco::Event _loginEnded{Poco::Event::EVENT_MANUALRESET};
		/// Set when the shell session starts or the connection is destroyed.
	Poco::Thread _loginTimer;
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_SSHShellConnection_INCLUDED
