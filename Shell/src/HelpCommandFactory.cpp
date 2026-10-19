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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/HelpCommandFactory.h"
#include "Poco/OSP/Shell/HelpCommand.h"


namespace Poco {
namespace OSP {
namespace Shell {


HelpCommandFactory::HelpCommandFactory(CommandProcessor& commandProcessor):
	_commandProcessor(commandProcessor)
{
}


HelpCommandFactory::~HelpCommandFactory()
{
}


Command* HelpCommandFactory::createCommand()
{
	return new HelpCommand(context(), _commandProcessor);
}


} } } // namespace Poco::OSP::Shell
