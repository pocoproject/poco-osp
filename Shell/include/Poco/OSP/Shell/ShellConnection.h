//
// ShellConnection.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellConnection
//
// Definition of the ShellConnection class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_ShellConnection_INCLUDED
#define OSP_Shell_ShellConnection_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/Net/TCPServerConnection.h"
#include <atomic>
#include <chrono>
#include <string>


namespace Poco::OSP::Shell {


class CommandProcessor;
class LoginFailures;
class SessionTracker;


class OSPShell_API ShellConnection: public Poco::Net::TCPServerConnection
	/// The ShellConnection class receives commands over a TCP connection
	/// and sends responses back to the client.
	///
	/// Commands are received one per line, and responses are sent in
	/// RFC 2821-style response chunks. Every command can send
	/// multiple response chunks. Intermediate response chunks have a
	/// status code of 100, and can occur more than once.
	/// The final chunk has a status code has a 200, 4xx or 500 status code
	/// and is exactly one line long.
	/// If the command was successful, the status code is 200 and the
	/// response text is "OK". If the command was not successful
	/// (its return code was not 0), the status code is 400 + return code.
	/// If the comand resulted in an exception, the status code is 500.
	///
	/// Example (positive response to help command):
	///     100-The following commands are available:
	///     100-bundles (ls, ss)
	///     100-services (lss)
	///     100-start
	///     100-stop
	///     ...
	///     100-set
	///     100 Enter <command> --help for more information about a command
	///     200 OK
	///
	/// Example (negative response to login command):
	///     403 DENIED
	///
	/// A rejected login is logged with the client address and answered after
	/// the delay that LoginFailures determines for the address. The connection
	/// is closed after MAX_FAILED_LOGINS rejected logins.
	///
	/// A session in which no command arrives within the session timeout is closed.
{
public:
	enum StatusCode
	{
		STATUS_INTERMEDIATE  = 100,
		STATUS_OK            = 200,
		STATUS_GOODBYE       = 201,
		STATUS_CLIENT_ERROR  = 400,
		STATUS_CLIENT_USAGE  = 401,
		STATUS_CLIENT_DENIED = 403, 
		STATUS_SERVER_ERROR  = 500
	};
	
	struct Config
		/// What the sessions of a shell server share. The objects must exist as
		/// long as the sessions.
	{
		CommandProcessor& commandProcessor;
			/// The command processor that runs the commands.

		std::string authServiceName;
			/// The name of the authentication service; empty to disable authentication.

		LoginFailures& loginFailures;
			/// The rejected logins per client address.

		std::chrono::seconds sessionTimeout;
			/// The time after which a session without a command is closed.
	};

	ShellConnection(const Poco::Net::StreamSocket& s, const Config& config, SessionTracker& sessions);
		/// Creates the ShellConnection and adds its socket to the given
		/// SessionTracker, which must exist as long as the connection.
		
	~ShellConnection() override;
		/// Destroys the ShellConnection.

	ShellConnection() = delete;
		
	// TCPServerConnection
	void run() override;

	static constexpr int MAX_FAILED_LOGINS = 3;
		/// The number of rejected logins after which the connection is closed.

protected:
	void onBundleStopping(const void* pSender, Poco::OSP::BundleEvent& ev);
		/// Close the underlying socket if our bundle is stopping.
	
private:
	[[nodiscard]] bool stopped() const;
		/// Returns true once the bundle of the shell is stopping or the
		/// SessionTracker has been stopped.

	const Config _config;
	SessionTracker& _sessions;
	std::atomic<bool> _bundleStopping{false};
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_ShellConnection_INCLUDED
