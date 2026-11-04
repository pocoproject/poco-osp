//
// ShellClient.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ShellClient.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/Socket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include <chrono>
#include <cstddef>
#include <string>


using Poco::OSP::Shell::ShellConnection;


namespace
{
	constexpr std::chrono::seconds RECEIVE_TIMEOUT(30);

	std::string withoutStatusCode(const std::string& line)
		/// Returns the line without the status code and the separator in front of it.
	{
		constexpr std::size_t PREFIX_LENGTH = 4;
		return line.size() > PREFIX_LENGTH ? line.substr(PREFIX_LENGTH) : std::string();
	}

	void appendLines(const std::string& message, std::string& text)
		/// Appends the lines of a status message to text, without their status
		/// codes, each followed by a newline.
	{
		const Poco::StringTokenizer lines(message, "\n");
		for (const auto& line: lines)
		{
			text += withoutStatusCode(line);
			text += '\n';
		}
	}
}


ShellClient::ShellClient(Poco::UInt16 port):
	_socket(Poco::Net::SocketAddress("127.0.0.1", port))
{
	_socket.setReceiveTimeout(Poco::Timespan(RECEIVE_TIMEOUT));
}


ShellClient::~ShellClient() = default;


ShellClient::Response ShellClient::receive()
{
	Response response;
	std::string message;
	response.status = _socket.receiveStatusMessage(message);
	while (response.status == ShellConnection::STATUS_INTERMEDIATE)
	{
		appendLines(message, response.text);
		response.status = _socket.receiveStatusMessage(message);
	}
	// The last line of the final block carries the result.
	const std::size_t lastLine = message.rfind('\n');
	if (lastLine == std::string::npos)
	{
		response.final = withoutStatusCode(message);
	}
	else
	{
		appendLines(message.substr(0, lastLine), response.text);
		response.final = withoutStatusCode(message.substr(lastLine + 1));
	}
	return response;
}


ShellClient::Response ShellClient::send(const std::string& commandLine)
{
	sendLine(commandLine);
	return receive();
}


void ShellClient::sendLine(const std::string& commandLine)
{
	_socket.sendMessage(commandLine);
}


void ShellClient::sendBytes(const std::string& data)
{
	_socket.sendBytes(data.data(), static_cast<int>(data.size()));
}


bool ShellClient::poll(std::chrono::milliseconds timeout)
{
	return _socket.poll(Poco::Timespan(timeout), Poco::Net::Socket::SELECT_READ);
}


bool ShellClient::closedByServer()
{
	try
	{
		char ch = 0;
		return _socket.receiveRawBytes(&ch, 1) == 0;
	}
	catch (Poco::Net::ConnectionResetException&)
	{
		return true;
	}
	catch (Poco::Net::ConnectionAbortedException&)
	{
		// Windows reports a connection that the server has reset after the
		// client sent more data as aborted.
		return true;
	}
}
