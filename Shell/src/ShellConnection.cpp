//
// ShellConnection.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellConnection
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Shell/ResponseStream.h"
#include "Poco/OSP/Shell/SessionTracker.h"
#include "ShellConnectionSupport.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/Net/Socket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketStream.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"
#include "Poco/Timespan.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <ostream>
#include <string>


using Poco::Delegate;


namespace Poco::OSP::Shell {


namespace
{
	bool receiveCommandLine(Poco::Net::StreamSocket& socket, std::chrono::seconds timeout,
		const std::function<bool()>& stopped, std::string& received, std::string& line)
		/// Receives the next command line, without its line end, as
		/// Poco::Net::DialogSocket::receiveMessage() does, but the whole line
		/// must arrive within the given time; with a timeout of zero, the wait
		/// has no limit. received keeps the data that follows the line.
		///
		/// Returns false if the client has closed the connection or stopped()
		/// returns true. Throws a Poco::TimeoutException if the line does not
		/// arrive in time, and a Poco::IOException if it is longer than
		/// MAX_COMMAND_LINE_LENGTH.
	{
		constexpr std::size_t RECEIVE_BUFFER_SIZE = 1024;
		// A receive that waits does not end on Windows when another thread shuts
		// the socket down, so the wait for data checks stopped() at this interval.
		constexpr std::chrono::milliseconds STOP_CHECK_INTERVAL{200};
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		for (;;)
		{
			const std::size_t end = received.find_first_of("\r\n");
			if ((end == std::string::npos ? received.size() : end) > MAX_COMMAND_LINE_LENGTH)
				throw Poco::IOException("Line too long");

			// A line feed after a carriage return belongs to the line end, so a
			// carriage return at the end of the received data needs the next character.
			if (end != std::string::npos && (received[end] == '\n' || end + 1 < received.size()))
			{
				line.assign(received, 0, end);
				const bool crlf = received[end] == '\r' && received[end + 1] == '\n';
				received.erase(0, end + (crlf ? 2 : 1));
				return true;
			}
			while (!socket.poll(Poco::Timespan(STOP_CHECK_INTERVAL), Poco::Net::Socket::SELECT_READ))
			{
				if (stopped()) return false;
				if (timeout.count() > 0 && std::chrono::steady_clock::now() >= deadline)
					throw Poco::TimeoutException();
			}
			if (timeout.count() > 0)
			{
				const auto remaining =
					std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now());
				if (remaining.count() <= 0) throw Poco::TimeoutException();
				socket.setReceiveTimeout(Poco::Timespan(remaining));
			}
			std::array<char, RECEIVE_BUFFER_SIZE> buffer{};
			const int length = socket.receiveBytes(buffer.data(), static_cast<int>(buffer.size()));
			if (length <= 0) return false;
			received.append(buffer.data(), static_cast<std::size_t>(length));
		}
	}
}


ShellConnection::ShellConnection(const Poco::Net::StreamSocket& s, const Config& config, SessionTracker& sessions):
	Poco::Net::TCPServerConnection(s),
	_config(config),
	_sessions(sessions)
{
	_config.commandProcessor.context()->events().bundleStopping +=
		Delegate<ShellConnection, BundleEvent>(this, &ShellConnection::onBundleStopping);
	_sessions.add(this->socket());
}


ShellConnection::~ShellConnection()
{
	try
	{
		_config.commandProcessor.context()->events().bundleStopping -=
			Delegate<ShellConnection, BundleEvent>(this, &ShellConnection::onBundleStopping);
		// Last: once the socket is removed, the shell may destroy what the session uses.
		_sessions.remove(this->socket());
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void ShellConnection::run()
{
	// Taken once: after the socket has been shut down, the system no longer reports the peer address.
	const Poco::Net::SocketAddress peerAddress = this->socket().peerAddress();
	_config.commandProcessor.context()->logger().information("New connection from " + peerAddress.toString());
	
	try
	{
		Session session(_config.commandProcessor.context(), _config.authServiceName);
		Poco::Net::SocketOutputStream ostr(this->socket());
		ResponseStream responseStream(ostr);
		
		responseStream.setStatus(STATUS_OK);
		std::string greeting(_config.commandProcessor.context()->thisBundle()->properties().getString("greeting", ""));
		if (!greeting.empty())
			responseStream << greeting << '\n' << std::flush;
		
		std::string received;
		std::string commandLine;
		bool keepOpen = true;
		const std::function<bool()> sessionStopped = [this]() { return stopped(); };
		while (keepOpen &&
			receiveCommandLine(this->socket(), _config.sessionTimeout, sessionStopped, received, commandLine))
		{
			try
			{
				const int failedLogins = session.failedLogins();
				const bool wasAuthenticated = session.authenticated();
				const std::string previousUser = session.user();
				responseStream.setStatus(STATUS_INTERMEDIATE);
				int exitCode = _config.commandProcessor.execute(session, commandLine, responseStream);
				if (session.failedLogins() > failedLogins)
				{
					handleRejectedLogin(_config, _sessions, peerAddress, session.failedLogins());
					keepOpen = session.failedLogins() < MAX_FAILED_LOGINS;
				}
				else if (session.authenticated() && (!wasAuthenticated || session.user() != previousUser))
				{
					_config.commandProcessor.context()->logger().information(
						"User " + session.user() + " logged in to the shell from " + peerAddress.toString() + ".");
				}
				std::string response;
				int status = statusFor(exitCode, response);
				responseStream.setStatus(status);
				responseStream << response << '\n' << std::flush;
				if (exitCode < 0) break;
			}
			catch (Poco::Exception& exc)
			{
				responseStream.setStatus(STATUS_SERVER_ERROR);
				responseStream << exc.displayText() << '\n' << std::flush;
			}
		}
	}
	catch (Poco::TimeoutException&)
	{
		logIdleSessionClosed(_config, peerAddress);
	}
	catch (Poco::Exception& exc)
	{
		std::string msg("Error handling connection from ");
		msg += peerAddress.toString();
		msg += ": ";
		msg += exc.displayText();
		_config.commandProcessor.context()->logger().error(msg);
	}
	_config.commandProcessor.context()->logger().information("Closing connection from " + peerAddress.toString());
}


void ShellConnection::onBundleStopping(const void* /*pSender*/, Poco::OSP::BundleEvent& ev)
{
	if (ev.bundle() == _config.commandProcessor.context()->thisBundle())
	{
		_bundleStopping = true;
		this->socket().shutdown();
	}
}


bool ShellConnection::stopped() const
{
	return _bundleStopping || _sessions.stopped();
}


} // namespace Poco::OSP::Shell
