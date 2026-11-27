//
// LoginCommandFactory.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  LoginCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/LoginCommandFactory.h"
#include "Poco/OSP/Shell/LoginCommand.h"


namespace Poco::OSP::Shell {


LoginCommandFactory::LoginCommandFactory() = default;


LoginCommandFactory::~LoginCommandFactory() = default;


Command* LoginCommandFactory::createCommand()
{
	return new LoginCommand(context());
}


} // namespace Poco::OSP::Shell
