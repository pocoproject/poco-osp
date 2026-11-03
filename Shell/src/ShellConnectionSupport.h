//
// ShellConnectionSupport.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellConnectionSupport
//
// Functions that the TCP and the SSH sessions of the shell share.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_ShellConnectionSupport_INCLUDED
#define OSP_Shell_ShellConnectionSupport_INCLUDED


#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/LoginFailures.h"
#include "Poco/OSP/Shell/SessionTracker.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Types.h"
#include <chrono>
#include <cstddef>
#include <string>


namespace Poco::OSP::Shell {


inline constexpr std::size_t MAX_COMMAND_LINE_LENGTH = 4096;
	/// The longest command line that a session accepts.


// The functions are inline, so that the shell library and the shell bundle,
// which runs the SSH sessions, both have them without exporting them.


[[nodiscard]] int statusFor(int exitCode, std::string& response);
	/// Returns the status code of the final response line for the given
	/// exit code of a command, and stores the text of that line in response.

void handleRejectedLogin(const ShellConnection::Config& config, SessionTracker& sessions,
	const Poco::Net::SocketAddress& clientAddress, int rejectedLogins);
	/// Logs a rejected login from the given client address, the given number
	/// of rejected logins in its connection, and waits for the delay before
	/// the rejection is answered, or until the sessions are stopped.

void logIdleSessionClosed(const ShellConnection::Config& config, const Poco::Net::SocketAddress& clientAddress);
	/// Logs that the session from the given client address is closed because
	/// no command has arrived within the session timeout.


//
// inlines
//
inline int statusFor(int exitCode, std::string& response)
{
	switch (exitCode)
	{
	case Command::STATUS_OK:
		response = "OK";
		return ShellConnection::STATUS_OK;
	case Command::STATUS_USAGE:
		response = "USAGE";
		return ShellConnection::STATUS_CLIENT_USAGE;
	case Command::STATUS_ERROR:
		response = "ERROR";
		return ShellConnection::STATUS_CLIENT_ERROR;
	case Command::STATUS_DENIED:
		response = "DENIED";
		return ShellConnection::STATUS_CLIENT_DENIED;
	default:
		if (exitCode < 0)
		{
			response = "GOODBYE";
			return ShellConnection::STATUS_GOODBYE;
		}
		else
		{
			response = "ERROR";
			return ShellConnection::STATUS_CLIENT_ERROR + exitCode;
		}
	}
}


inline void handleRejectedLogin(const ShellConnection::Config& config, SessionTracker& sessions,
	const Poco::Net::SocketAddress& clientAddress, int rejectedLogins)
{
	const std::chrono::milliseconds delay = config.loginFailures.recordFailure(clientAddress.host());
	std::string msg("Shell login from ");
	msg += clientAddress.toString();
	msg += " rejected; the rejection is answered after ";
	msg += Poco::NumberFormatter::format(static_cast<Poco::Int64>(delay.count()));
	msg += " ms";
	if (rejectedLogins >= ShellConnection::MAX_FAILED_LOGINS)
	{
		msg += ", then the connection is closed after ";
		msg += Poco::NumberFormatter::format(ShellConnection::MAX_FAILED_LOGINS);
		msg += " rejected logins";
	}
	msg += '.';
	config.commandProcessor.context()->logger().warning(msg);
	sessions.waitForStop(delay);
}


inline void logIdleSessionClosed(const ShellConnection::Config& config, const Poco::Net::SocketAddress& clientAddress)
{
	std::string msg("Closing the shell session from ");
	msg += clientAddress.toString();
	msg += ": no command within ";
	msg += Poco::NumberFormatter::format(static_cast<Poco::Int64>(config.sessionTimeout.count()));
	msg += " seconds.";
	config.commandProcessor.context()->logger().information(msg);
}


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_ShellConnectionSupport_INCLUDED
