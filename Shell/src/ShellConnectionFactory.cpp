//
// ShellConnectionFactory.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellConnection
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/ShellConnectionFactory.h"
#include "Poco/OSP/Shell/ShellConnection.h"


namespace Poco::OSP::Shell {


ShellConnectionFactory::ShellConnectionFactory(const ShellConnection::Config& config, SessionTracker& sessions):
	_config(config),
	_sessions(sessions)
{
}


ShellConnectionFactory::~ShellConnectionFactory() = default;


Poco::Net::TCPServerConnection* ShellConnectionFactory::createConnection(const Poco::Net::StreamSocket& s)
{
	return new ShellConnection(s, _config, _sessions);
}


} // namespace Poco::OSP::Shell
