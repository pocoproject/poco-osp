//
// QuitCommandFactory.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  QuitCommand
//
// Definition of the QuitCommandFactory class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_QuitCommandFactory_INCLUDED
#define OSP_Shell_QuitCommandFactory_INCLUDED


#include "Poco/OSP/Shell/CommandFactory.h"


namespace Poco::OSP::Shell {


class OSPShell_API QuitCommandFactory: public CommandFactory
	/// The factory for QuitCommand objects.
{
public:
	QuitCommandFactory();
		/// Creates the QuitCommandFactory.
		
	~QuitCommandFactory() override;
		/// Destroys the QuitCommandFactory.
		
	// CommandFactory
	[[nodiscard]] Command* createCommand() override;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_QuitCommandFactory_INCLUDED
