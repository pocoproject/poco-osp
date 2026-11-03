//
// LoginCommandFactory.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  LoginCommand
//
// Definition of the LoginCommandFactory class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_LoginCommandFactory_INCLUDED
#define OSP_Shell_LoginCommandFactory_INCLUDED


#include "Poco/OSP/Shell/CommandFactory.h"


namespace Poco::OSP::Shell {


class OSPShell_API LoginCommandFactory: public CommandFactory
	/// The factory for LoginCommand objects.
{
public:
	LoginCommandFactory();
		/// Creates the LoginCommandFactory.
		
	~LoginCommandFactory() override;
		/// Destroys the LoginCommandFactory.
		
	// CommandFactory
	[[nodiscard]] Command* createCommand() override;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_LoginCommandFactory_INCLUDED
