//
// FakeSMTPServer.h
//
// Definition of the FakeSMTPServer class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef FakeSMTPServer_INCLUDED
#define FakeSMTPServer_INCLUDED


#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Event.h"
#include "Poco/Mutex.h"
#include "Poco/Runnable.h"
#include "Poco/Thread.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include <functional>
#include <string>
#include <vector>


class FakeSMTPServer: public Poco::Runnable
	/// An SMTP server on 127.0.0.1 (by default) that handles one connection at a time,
	/// accepts every login, sender and recipient, and records the commands
	/// it receives.
{
public:
	using SecureFunction = std::function<Poco::Net::StreamSocket(const Poco::Net::StreamSocket&)>;
		/// Turns an accepted connection into a TLS connection (server side).

	enum class TLSMode
	{
		NONE,       /// STARTTLS is not offered and answered with 502.
		STARTTLS,   /// STARTTLS is offered and starts TLS with the SecureFunction.
		ON_CONNECT  /// Every connection starts with TLS (SMTP over TLS).
	};

	FakeSMTPServer();
		/// Creates and starts the server without TLS.

	FakeSMTPServer(TLSMode mode, SecureFunction secure);
		/// Creates and starts the server with the given TLS mode.

	FakeSMTPServer(TLSMode mode, SecureFunction secure, const std::string& host);
		/// Creates and starts the server with the given TLS mode, on the address
		/// that a client connecting to the given host name (a name of a loopback
		/// address, such as localhost) connects to, instead of 127.0.0.1.

	~FakeSMTPServer() override;
		/// Stops the server.

	FakeSMTPServer(const FakeSMTPServer&) = delete;
	FakeSMTPServer(FakeSMTPServer&&) = delete;
	FakeSMTPServer& operator = (const FakeSMTPServer&) = delete;
	FakeSMTPServer& operator = (FakeSMTPServer&&) = delete;

	[[nodiscard]] Poco::UInt16 port() const;
		/// Returns the port the server listens on.

	void setDropAfterMessages(int count);
		/// Closes each connection without an answer after it has accepted count messages.

	void setGreetingDelay(const Poco::Timespan& delay);
		/// Waits the given time after accepting a connection before sending the greeting.

	void setSilent(bool silent);
		/// If silent, the server accepts connections and never sends anything.

	void setRejectLogin(bool reject);
		/// If reject, every AUTH command is answered with 535.

	[[nodiscard]] std::vector<std::string> commands() const;
		/// Returns the commands received so far, without the message contents.

	[[nodiscard]] std::vector<std::string> messageData() const;
		/// Returns the data of the accepted messages, with lines separated by CRLF.

	[[nodiscard]] bool hasReceived(const std::string& commandPrefix) const;
		/// Returns true if a command starting with commandPrefix has been received.

	[[nodiscard]] int connections() const;
		/// Returns the number of accepted connections.

	[[nodiscard]] int messages() const;
		/// Returns the number of accepted messages.

	[[nodiscard]] bool waitForConnections(int count);
		/// Waits up to ten seconds until count connections have been accepted.

	// Runnable
	void run() override;

private:
	void handleConnection(Poco::Net::StreamSocket socket);
	[[nodiscard]] bool waitFor(const std::function<bool()>& condition);

	Poco::Net::ServerSocket _serverSocket;
	TLSMode _tlsMode;
	SecureFunction _secure;
	Poco::Thread _thread;
	Poco::Event _stop{Poco::Event::EVENT_MANUALRESET};
	Poco::Event _changed;
	mutable Poco::FastMutex _mutex;
	std::vector<std::string> _commands;
	std::vector<std::string> _messageData;
	int _connections = 0;
	int _messages = 0;
	int _dropAfterMessages = 0;
	Poco::Timespan _greetingDelay;
	bool _silent = false;
	bool _rejectLogin = false;
};


#endif // FakeSMTPServer_INCLUDED
