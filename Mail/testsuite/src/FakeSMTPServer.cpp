//
// FakeSMTPServer.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "FakeSMTPServer.h"
#include "Poco/Net/DialogSocket.h"
#include "Poco/Net/Socket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Bugcheck.h"
#include "Poco/Exception.h"
#include "Poco/Mutex.h"
#include "Poco/String.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <vector>


using namespace std::string_literals;


namespace
{
	const Poco::Timespan POLL_INTERVAL(0, 100000);
	const Poco::Timespan RECEIVE_TIMEOUT(10, 0);
	constexpr long WAIT_MILLISECONDS = 10000;
}


FakeSMTPServer::FakeSMTPServer():
	FakeSMTPServer(TLSMode::NONE, SecureFunction())
{
}


FakeSMTPServer::FakeSMTPServer(TLSMode mode, SecureFunction secure):
	FakeSMTPServer(mode, std::move(secure), "127.0.0.1"s)
{
}


FakeSMTPServer::FakeSMTPServer(TLSMode mode, SecureFunction secure, const std::string& host):
	_serverSocket(Poco::Net::SocketAddress(host, 0)),
	_tlsMode(mode),
	_secure(std::move(secure))
{
	_thread.start(*this);
}


FakeSMTPServer::~FakeSMTPServer()
{
	try
	{
		_stop.set();
		_thread.join();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


Poco::UInt16 FakeSMTPServer::port() const
{
	return _serverSocket.address().port();
}


void FakeSMTPServer::setDropAfterMessages(int count)
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	_dropAfterMessages = count;
}


void FakeSMTPServer::setGreetingDelay(const Poco::Timespan& delay)
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	_greetingDelay = delay;
}


void FakeSMTPServer::setSilent(bool silent)
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	_silent = silent;
}


void FakeSMTPServer::setRejectLogin(bool reject)
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	_rejectLogin = reject;
}


std::vector<std::string> FakeSMTPServer::commands() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	return _commands;
}


std::vector<std::string> FakeSMTPServer::messageData() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	return _messageData;
}


bool FakeSMTPServer::hasReceived(const std::string& commandPrefix) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	for (const auto& command: _commands)
	{
		if (Poco::icompare(command, 0, commandPrefix.size(), commandPrefix) == 0) return true;
	}
	return false;
}


int FakeSMTPServer::connections() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	return _connections;
}


int FakeSMTPServer::messages() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	return _messages;
}


bool FakeSMTPServer::waitForConnections(int count)
{
	return waitFor([this, count]() { return connections() >= count; });
}


bool FakeSMTPServer::waitFor(const std::function<bool()>& condition)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(WAIT_MILLISECONDS);
	while (!condition())
	{
		const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if (remaining.count() <= 0) return false;
		_changed.tryWait(static_cast<long>(remaining.count()));
	}
	return true;
}


void FakeSMTPServer::run()
{
	while (!_stop.tryWait(0))
	{
		try
		{
			if (_serverSocket.poll(POLL_INTERVAL, Poco::Net::Socket::SELECT_READ))
			{
				handleConnection(_serverSocket.acceptConnection());
			}
		}
		catch (Poco::Exception&)
		{
			// The client has closed the connection or the TLS handshake has failed.
		}
	}
}


void FakeSMTPServer::handleConnection(Poco::Net::StreamSocket socket)
{
	Poco::Timespan greetingDelay;
	bool silent = false;
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		++_connections;
		greetingDelay = _greetingDelay;
		silent = _silent;
	}
	_changed.set();

	if (silent)
	{
		_stop.wait();
		return;
	}
	if (greetingDelay > 0 && _stop.tryWait(static_cast<long>(greetingDelay.totalMilliseconds()))) return;

	bool secure = false;
	if (_tlsMode == TLSMode::ON_CONNECT)
	{
		socket = _secure(socket);
		secure = true;
	}
	Poco::Net::DialogSocket dialog(socket);
	dialog.setReceiveTimeout(RECEIVE_TIMEOUT);
	dialog.sendMessage("220 localhost ESMTP"s);

	int messagesOnConnection = 0;
	std::string line;
	while (dialog.receiveMessage(line))
	{
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			_commands.push_back(line);
		}
		_changed.set();
		const std::string command = Poco::toUpper(line.substr(0, 4));
		if (command == "EHLO")
		{
			dialog.sendMessage("250-localhost"s);
			if (_tlsMode == TLSMode::STARTTLS && !secure) dialog.sendMessage("250-STARTTLS"s);
			dialog.sendMessage("250 AUTH PLAIN LOGIN"s);
		}
		else if (command == "HELO")
		{
			dialog.sendMessage("250 localhost"s);
		}
		else if (command == "STAR")
		{
			if (_tlsMode == TLSMode::STARTTLS && !secure)
			{
				dialog.sendMessage("220 Ready to start TLS"s);
				socket = _secure(socket);
				dialog = socket;
				dialog.setReceiveTimeout(RECEIVE_TIMEOUT);
				secure = true;
			}
			else dialog.sendMessage("502 Command not implemented"s);
		}
		else if (command == "AUTH")
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			dialog.sendMessage(_rejectLogin ? "535 Authentication failed"s : "235 Authentication successful"s);
		}
		else if (command == "DATA")
		{
			dialog.sendMessage("354 End data with <CR><LF>.<CR><LF>"s);
			std::string data;
			while (dialog.receiveMessage(line) && line != ".")
			{
				data += line;
				data += "\r\n";
			}
			++messagesOnConnection;
			int dropAfterMessages = 0;
			{
				Poco::FastMutex::ScopedLock lock(_mutex);
				++_messages;
				_messageData.push_back(data);
				dropAfterMessages = _dropAfterMessages;
			}
			_changed.set();
			// Recorded before the answer: the client reports the message as
			// delivered as soon as it receives the answer.
			dialog.sendMessage("250 Queued"s);
			if (dropAfterMessages > 0 && messagesOnConnection >= dropAfterMessages) return;
		}
		else if (command == "QUIT")
		{
			dialog.sendMessage("221 Bye"s);
			return;
		}
		else
		{
			dialog.sendMessage("250 OK"s);
		}
	}
}
