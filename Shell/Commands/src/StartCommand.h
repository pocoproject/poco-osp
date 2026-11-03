//
// StartCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  StartCommand
//
// Definition of the StartCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_StartCommand_INCLUDED
#define OSP_Shell_StartCommand_INCLUDED


#include "BundleActionCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco::OSP::Shell {


class StartCommand: public BundleActionCommand
	/// This command starts a bundle.
{
public:
	StartCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the StartCommand, using the given
		/// bundle context.
		
	~StartCommand() override;
		/// Destroys the StartCommand.

protected:
	// BundleActionCommand
	[[nodiscard]] int doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr) override;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_StartCommand_INCLUDED
