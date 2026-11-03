//
// ShellConnectionFactory.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellConnection
//
// Definition of the ShellConnectionFactory class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_ShellConnectionFactory_INCLUDED
#define OSP_Shell_ShellConnectionFactory_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/TCPServerConnectionFactory.h"


namespace Poco::OSP::Shell {


class SessionTracker;


class OSPShell_API ShellConnectionFactory: public Poco::Net::TCPServerConnectionFactory
	/// The factory for ShellConnection objects.
{
public:
	ShellConnectionFactory(const ShellConnection::Config& config, SessionTracker& sessions);
		/// Creates the ShellConnectionFactory for connections with the given
		/// config and SessionTracker.
		
	~ShellConnectionFactory() override;
		/// Destroys the ShellConnectionFactory.

	ShellConnectionFactory() = delete;
		
	// TCPServerConnectionFactory
	[[nodiscard]] Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override;

private:
	ShellConnection::Config _config;
	SessionTracker& _sessions;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_ShellConnectionFactory_INCLUDED
