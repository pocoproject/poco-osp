//
// HelpCommandFactory.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  HelpCommand
//
// Definition of the HelpCommandFactory class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_HelpCommandFactory_INCLUDED
#define OSP_Shell_HelpCommandFactory_INCLUDED


#include "Poco/OSP/Shell/CommandFactory.h"


namespace Poco::OSP::Shell {


class CommandProcessor;


class OSPShell_API HelpCommandFactory: public CommandFactory
	/// The factory for HelpCommand objects.
{
public:
	HelpCommandFactory(CommandProcessor& commandProcessor);
		/// Creates the HelpCommandFactory, using the given command processor.
		
	~HelpCommandFactory() override;
		/// Destroys the HelpCommandFactory.
		
	// CommandFactory
	[[nodiscard]] Command* createCommand() override;

private:
	CommandProcessor& _commandProcessor;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_HelpCommandFactory_INCLUDED
