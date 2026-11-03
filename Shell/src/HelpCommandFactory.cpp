//
// HelpCommandFactory.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  HelpCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/HelpCommandFactory.h"
#include "Poco/OSP/Shell/HelpCommand.h"


namespace Poco::OSP::Shell {


HelpCommandFactory::HelpCommandFactory(CommandProcessor& commandProcessor):
	_commandProcessor(commandProcessor)
{
}


HelpCommandFactory::~HelpCommandFactory() = default;


Command* HelpCommandFactory::createCommand()
{
	return new HelpCommand(context(), _commandProcessor);
}


} // namespace Poco::OSP::Shell
