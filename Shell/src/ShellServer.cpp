//
// ShellServer.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellServer
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/ShellServer.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Shell/ShellConnectionFactory.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Types.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>


namespace Poco {
namespace OSP {
namespace Shell {


namespace
{
	Poco::Net::TCPServerParams::Ptr serverParams(int maxConnections)
	{
		Poco::Net::TCPServerParams::Ptr pParams = new Poco::Net::TCPServerParams;
		pParams->setMaxThreads(maxConnections);
		pParams->setMaxQueued(1);
		return pParams;
	}
}


ShellServer::ShellServer(const Poco::Net::ServerSocket& socket, int maxConnections,
	const ShellConnection::Config& config):
	_config(config),
	_threadPool(1, std::max(1, maxConnections)),
	_socket(socket),
	_server(new ShellConnectionFactory(_config, _sessions), _threadPool, _socket, serverParams(maxConnections))
{
}


ShellServer::~ShellServer() = default;


void ShellServer::start()
{
	_server.start();
}


std::size_t ShellServer::stop(std::chrono::milliseconds timeout)
{
	_server.stop();
	// TCPServer::stop() leaves the socket open; a server that is kept for
	// sessions that have not ended would otherwise still queue connections.
	_socket.close();
	_sessions.stop();
	const std::size_t activeSessions = _sessions.waitUntilEnded(timeout);
	if (activeSessions == 0)
	{
		_threadPool.joinAll();
	}
	else
	{
		std::string msg = Poco::NumberFormatter::format(activeSessions);
		msg += " shell sessions have not ended within ";
		msg += Poco::NumberFormatter::format(static_cast<Poco::Int64>(timeout.count()));
		msg += " ms after the shell stopped; the objects they use are kept.";
		_config.commandProcessor.context()->logger().warning(msg);
	}
	return activeSessions;
}


} } } // namespace Poco::OSP::Shell
