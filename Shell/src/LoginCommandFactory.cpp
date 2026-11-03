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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
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
