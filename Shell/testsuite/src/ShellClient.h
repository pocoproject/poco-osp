//
// ShellClient.h
//
// Definition of the ShellClient class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ShellClient_INCLUDED
#define ShellClient_INCLUDED


#include "Poco/Net/DialogSocket.h"
#include "Poco/Types.h"
#include <chrono>
#include <string>


class ShellClient
	/// A connection to a shell server on 127.0.0.1.
	///
	/// A receive that waits longer than 30 seconds throws a Poco::TimeoutException,
	/// so that a server that does not answer fails the test instead of blocking it.
{
public:
	struct Response
	{
		int status = 0;
			/// The status code of the final line.
		std::string text;
			/// The lines before the final line, without the status codes,
			/// each followed by a newline.
		std::string final;
			/// The text of the final line, such as "OK" or "DENIED".
	};

	ShellClient() = delete;

	explicit ShellClient(Poco::UInt16 port);
		/// Connects to the shell server at 127.0.0.1 and the given port.

	~ShellClient();
		/// Closes the connection.

	ShellClient(const ShellClient&) = delete;
	ShellClient(ShellClient&&) = delete;
	ShellClient& operator = (const ShellClient&) = delete;
	ShellClient& operator = (ShellClient&&) = delete;

	[[nodiscard]] Response receive();
		/// Receives the response that the server sends next: the greeting
		/// after the connection has been established, or the response to a
		/// command.

	[[nodiscard]] Response send(const std::string& commandLine);
		/// Sends the command line and returns the response.

	void sendLine(const std::string& commandLine);
		/// Sends the command line without waiting for the response.

	void sendBytes(const std::string& data);
		/// Sends the data as it is, without a line end.

	[[nodiscard]] bool poll(std::chrono::milliseconds timeout);
		/// Returns true if data or the end of the connection arrives within
		/// the given time. Data that receive() has buffered is not seen.

	[[nodiscard]] bool closedByServer();
		/// Returns true if the server closes the connection before it sends
		/// anything else, false if data arrives.

private:
	Poco::Net::DialogSocket _socket;
};


#endif // ShellClient_INCLUDED
