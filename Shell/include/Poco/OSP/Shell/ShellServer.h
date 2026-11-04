//
// ShellServer.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellServer
//
// Definition of the ShellServer class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_ShellServer_INCLUDED
#define OSP_Shell_ShellServer_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/OSP/Shell/SessionTracker.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/ThreadPool.h"
#include "Poco/Types.h"
#include <chrono>
#include <cstddef>


namespace Poco {
namespace OSP {
namespace Shell {


class OSPShell_API ShellServer
	/// ShellServer is the TCP server of the shell. It runs each session in a
	/// thread of a thread pool of its own, so that stop() can wait until the
	/// sessions have ended.
{
public:
	ShellServer() = delete;

	ShellServer(const Poco::Net::ServerSocket& socket, int maxConnections, const ShellConnection::Config& config);
		/// Creates the ShellServer for the given server socket, with at most
		/// maxConnections sessions at a time. The objects that the config
		/// refers to must exist until stop() has returned 0.

	~ShellServer();
		/// Destroys the ShellServer. stop() must have returned 0 before.

	ShellServer(const ShellServer&) = delete;
	ShellServer(ShellServer&&) = delete;
	ShellServer& operator = (const ShellServer&) = delete;
	ShellServer& operator = (ShellServer&&) = delete;

	void start();
		/// Starts accepting connections.

	[[nodiscard]] std::size_t stop(std::chrono::milliseconds timeout);
		/// Stops accepting connections, closes the server socket, shuts down
		/// the sockets of the active sessions and waits until the sessions
		/// have ended, at most for the given time. Returns the number of
		/// sessions that have not ended; these are logged as a warning. Such
		/// sessions still use the ShellServer and the objects that the config
		/// refers to.

	[[nodiscard]] Poco::UInt16 port() const;
		/// Returns the port number on which the server accepts connections.

private:
	ShellConnection::Config _config;
	SessionTracker _sessions;
	Poco::ThreadPool _threadPool;
	Poco::Net::ServerSocket _socket;
	Poco::Net::TCPServer _server;
};


//
// inlines
//
inline Poco::UInt16 ShellServer::port() const
{
	return _server.port();
}


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_ShellServer_INCLUDED
