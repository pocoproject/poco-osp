//
// SSHShellConnection.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  SSHShellConnection
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//


#include "SSHShellConnection.h"
#include "ShellConnectionSupport.h"
#include "Poco/SSH/SSHChannelStream.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Shell/ResponseStream.h"
#include "Poco/OSP/Shell/SessionTracker.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/SSH/SSHServer.h"
#include "Poco/SSH/SSHSession.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketDefs.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/StreamSocketImpl.h"
#include "Poco/Net/NetException.h"
#include "Poco/Bugcheck.h"
#include "Poco/Error.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Thread.h"
#include "Poco/Types.h"
#include <libssh/libssh.h>
#if !defined(POCO_OS_FAMILY_WINDOWS)
#include <fcntl.h>
#endif
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>


namespace Poco::OSP::Shell {


namespace
{
	Poco::Net::StreamSocket duplicateSocket(ssh_session session)
		/// Returns a second descriptor of the connection of the session. As the
		/// sockets of POCO, it is not inherited by child processes.
	{
#if defined(POCO_OS_FAMILY_WINDOWS)
		WSAPROTOCOL_INFOW info{};
		const poco_socket_t duplicate = ::WSADuplicateSocketW(ssh_get_fd(session), ::GetCurrentProcessId(), &info) == 0
			? ::WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &info, 0,
				WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT)
			: POCO_INVALID_SOCKET;
#else
		const poco_socket_t duplicate = ::fcntl(ssh_get_fd(session), F_DUPFD_CLOEXEC, 0);
#endif
		if (duplicate == POCO_INVALID_SOCKET)
		{
			throw Poco::Net::NetException("Cannot duplicate the socket of an SSH connection",
				Poco::Error::getMessage(Poco::Error::last()));
		}
		return Poco::Net::StreamSocket(new Poco::Net::StreamSocketImpl(duplicate));
	}
}


SSHShellConnection::SSHShellConnection(
	ssh_session session,
	const Poco::SSH::SSHServerConfig& config,
	Poco::SSH::SSHServer& server,
	const ShellConnection::Config& shellConfig,
	SessionTracker& sessions,
	std::chrono::seconds loginGraceTime):
	SSHShellConnection(session, config, server, shellConfig, sessions, loginGraceTime, std::make_shared<std::string>())
{
}


SSHShellConnection::SSHShellConnection(
	ssh_session session,
	const Poco::SSH::SSHServerConfig& config,
	Poco::SSH::SSHServer& server,
	const ShellConnection::Config& shellConfig,
	SessionTracker& sessions,
	std::chrono::seconds loginGraceTime,
	const std::shared_ptr<std::string>& pAccountName):
	Poco::SSH::SSHSession(session,
		withLoginChecks(config, clientAddressOf(session), shellConfig, sessions, pAccountName), server),
	_shellConfig(shellConfig),
	_sessions(sessions),
	_pAccountName(pAccountName),
	_clientAddress(clientAddressOf(session)),
	_socket(duplicateSocket(session))
{
	if (loginGraceTime.count() > 0)
	{
		_loginTimer.startFunc([this, loginGraceTime]()
		{
			closeWithoutLogin(loginGraceTime);
		});
	}
}


SSHShellConnection::~SSHShellConnection()
{
	try
	{
		_loginEnded.set();
		_loginTimer.join();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


bool SSHShellConnection::stopServer(Poco::SSH::SSHServer& server, SessionTracker& sessions, Poco::Thread& stopper,
	std::chrono::milliseconds timeout)
{
	sessions.stop();
	// SSHServer::stop() waits for the sessions without a time limit.
	stopper.startFunc([&server]()
	{
		server.stop();
	});
	return stopper.tryJoin(static_cast<long>(timeout.count()));
}


void SSHShellConnection::handleSession(ssh_channel channel, const std::string& sshUserName)
{
	{
		Poco::FastMutex::ScopedLock lock(_sessionMutex);
		_sessionStarted = true;
	}
	_loginEnded.set();
	// A public key login has no account name from the authentication service,
	// so its session keeps the SSH user name.
	const std::string& accountName = _pAccountName->empty() ? sshUserName : *_pAccountName;
	Session session(_shellConfig.commandProcessor.context(), _shellConfig.authServiceName);
	session.loginWithKey(accountName);
	Poco::Logger& logger = _shellConfig.commandProcessor.context()->logger();
	logger.information(
		"User " + accountName + " logged in to the shell over SSH from " + _clientAddress.toString() + ".");

	Poco::SSH::SSHChannelStream outStream(channel);
	ResponseStream responseStream(outStream);

	// Send greeting
	std::string greeting =
		_shellConfig.commandProcessor.context()->thisBundle()->properties().getString("greeting", "");
	if (!greeting.empty())
	{
		responseStream.setStatus(200 /*STATUS_OK*/);
		responseStream << greeting << std::endl;
	}

	// Send initial prompt
	outStream << "osp> " << std::flush;

	std::string commandLine;
	bool keepOpen = true;
	while (keepOpen)
	{
		const ReadResult result = readLine(channel, commandLine);
		if (result == ReadResult::TIMEOUT)
		{
			logIdleSessionClosed(_shellConfig, _clientAddress);
			break;
		}
		if (result == ReadResult::TOO_LONG)
		{
			std::string msg("Closing the shell session from ");
			msg += _clientAddress.toString();
			msg += ": a command line is longer than ";
			msg += Poco::NumberFormatter::format(MAX_COMMAND_LINE_LENGTH);
			msg += " characters.";
			logger.error(msg);
			break;
		}
		if (result == ReadResult::END) break;

		// Trim whitespace
		while (!commandLine.empty() && (commandLine.back() == ' ' || commandLine.back() == '\t'))
			commandLine.pop_back();

		if (commandLine.empty())
		{
			outStream << "osp> " << std::flush;
			continue;
		}

		try
		{
			const int failedLogins = session.failedLogins();
			responseStream.setStatus(100 /*STATUS_INTERMEDIATE*/);
			int exitCode = _shellConfig.commandProcessor.execute(session, commandLine, responseStream);
			if (session.failedLogins() > failedLogins)
			{
				handleRejectedLogin(_shellConfig, _sessions, _clientAddress, session.failedLogins());
				keepOpen = session.failedLogins() < ShellConnection::MAX_FAILED_LOGINS;
			}
			std::string response;
			const int status = statusFor(exitCode, response);
			responseStream.setStatus(status);
			responseStream << response << std::endl;

			if (exitCode < 0)
				break;
		}
		catch (Poco::Exception& exc)
		{
			responseStream.setStatus(500 /*STATUS_SERVER_ERROR*/);
			responseStream << exc.displayText() << std::endl;
		}

		outStream << "osp> " << std::flush;
	}
}


SSHShellConnection::ReadResult SSHShellConnection::readLine(ssh_channel channel, std::string& line) const
{
	// Line editing as in Poco::SSH::sshReadLine(), with a timeout and a limit
	// of the line length, as on the TCP transport.
	const auto deadline = std::chrono::steady_clock::now() + _shellConfig.sessionTimeout;
	line.clear();
	char ch = 0;
	for (;;)
	{
		int timeoutMilliseconds = -1;
		if (_shellConfig.sessionTimeout.count() > 0)
		{
			const auto remaining =
				std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
			if (remaining.count() <= 0) return ReadResult::TIMEOUT;
			timeoutMilliseconds = static_cast<int>(remaining.count());
		}
		const int length = ssh_channel_read_timeout(channel, &ch, 1, 0, timeoutMilliseconds);
		if (length == SSH_AGAIN ||
			(length == 0 && ssh_channel_is_open(channel) != 0 && ssh_channel_is_eof(channel) == 0))
			return ReadResult::TIMEOUT;
		if (length <= 0) return ReadResult::END;

		if (ch == '\r' || ch == '\n')
		{
			ssh_channel_write(channel, "\r\n", 2);
			return ReadResult::LINE;
		}
		if (ch == '\x7f' || ch == '\b')
		{
			if (!line.empty())
			{
				line.pop_back();
				ssh_channel_write(channel, "\b \b", 3);
			}
		}
		else if (ch == '\x03')
		{
			ssh_channel_write(channel, "^C\r\n", 4);
			line.clear();
			return ReadResult::LINE;
		}
		else if (static_cast<unsigned char>(ch) >= ' ')
		{
			if (line.size() >= MAX_COMMAND_LINE_LENGTH) return ReadResult::TOO_LONG;
			line += ch;
			ssh_channel_write(channel, &ch, 1);
		}
	}
}


Poco::Net::SocketAddress SSHShellConnection::clientAddressOf(ssh_session session)
{
	struct sockaddr_storage address{};
	poco_socklen_t length = sizeof(address);
	if (::getpeername(ssh_get_fd(session), reinterpret_cast<struct sockaddr*>(&address), &length) != 0)
	{
		return Poco::Net::SocketAddress();
	}
	return Poco::Net::SocketAddress(reinterpret_cast<const struct sockaddr*>(&address), length);
}


Poco::SSH::SSHServerConfig SSHShellConnection::withLoginChecks(
	const Poco::SSH::SSHServerConfig& config,
	const Poco::Net::SocketAddress& clientAddress,
	const ShellConnection::Config& shellConfig,
	SessionTracker& sessions,
	const std::shared_ptr<std::string>& pAccountName)
{
	Poco::SSH::SSHServerConfig result(config);
	result.maxAuthAttempts = std::min(config.maxAuthAttempts, ShellConnection::MAX_FAILED_LOGINS);
	auto pRejectedLogins = std::make_shared<int>(0);
	result.passwordAuthenticator = [shellConfig, &sessions, clientAddress, pRejectedLogins, pAccountName](
		const std::string& user, const std::string& password)
	{
		Session session(shellConfig.commandProcessor.context(), shellConfig.authServiceName);
		if (session.login(user, password))
		{
			*pAccountName = session.user();
			return true;
		}

		handleRejectedLogin(shellConfig, sessions, clientAddress, ++*pRejectedLogins);
		return false;
	};
	return result;
}


void SSHShellConnection::closeWithoutLogin(std::chrono::seconds loginGraceTime)
{
	const auto deadline = std::chrono::steady_clock::now() + loginGraceTime;
	for (;;)
	{
		const auto remaining =
			std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if (remaining.count() <= 0) break;
		if (_loginEnded.tryWait(static_cast<long>(remaining.count()))) return;
	}

	Poco::FastMutex::ScopedLock lock(_sessionMutex);
	if (_sessionStarted) return;

	std::string msg("Closing the SSH connection from ");
	msg += _clientAddress.toString();
	msg += ": no shell session within ";
	msg += Poco::NumberFormatter::format(static_cast<Poco::Int64>(loginGraceTime.count()));
	msg += " seconds; the client has not logged in or has not opened a shell.";
	_shellConfig.commandProcessor.context()->logger().information(msg);
	try
	{
		_socket.shutdown();
	}
	catch (Poco::Exception&)
	{
		// The client has closed the connection already, so the session ends anyway.
	}
}


} // namespace Poco::OSP::Shell
