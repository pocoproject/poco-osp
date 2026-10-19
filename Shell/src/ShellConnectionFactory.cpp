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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/ShellConnectionFactory.h"
#include "Poco/OSP/Shell/ShellConnection.h"


namespace Poco {
namespace OSP {
namespace Shell {


ShellConnectionFactory::ShellConnectionFactory(CommandProcessor& commandProcessor, const std::string& authServiceName):
	_commandProcessor(commandProcessor),
	_authServiceName(authServiceName)
{
}


ShellConnectionFactory::~ShellConnectionFactory()
{
}


Poco::Net::TCPServerConnection* ShellConnectionFactory::createConnection(const Poco::Net::StreamSocket& s)
{
	return new ShellConnection(s, _commandProcessor, _authServiceName);
}


} } } // namespace Poco::OSP::Shell
