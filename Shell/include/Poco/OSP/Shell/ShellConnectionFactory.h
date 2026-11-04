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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_ShellConnectionFactory_INCLUDED
#define OSP_Shell_ShellConnectionFactory_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/TCPServerConnectionFactory.h"


namespace Poco {
namespace OSP {
namespace Shell {


class SessionTracker;


class OSPShell_API ShellConnectionFactory: public Poco::Net::TCPServerConnectionFactory
	/// The factory for ShellConnection objects.
{
public:
	ShellConnectionFactory(const ShellConnection::Config& config, SessionTracker& sessions);
		/// Creates the ShellConnectionFactory for connections with the given
		/// config and SessionTracker.
		
	~ShellConnectionFactory();
		/// Destroys the ShellConnectionFactory.
		
	// TCPServerConnectionFactory
	Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket);

private:
	ShellConnectionFactory();
	
	ShellConnection::Config _config;
	SessionTracker& _sessions;
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_ShellConnectionFactory_INCLUDED
