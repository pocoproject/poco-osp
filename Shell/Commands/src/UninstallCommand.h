//
// UninstallCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  UninstallCommand
//
// Definition of the UninstallCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_UninstallCommand_INCLUDED
#define OSP_Shell_UninstallCommand_INCLUDED


#include "BundleActionCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco::OSP::Shell {


class UninstallCommand: public BundleActionCommand
	/// This command uninstalls a bundle.
{
public:
	UninstallCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the UninstallCommand, using the given
		/// bundle context.
		
	~UninstallCommand() override;
		/// Destroys the UninstallCommand.

protected:
	// BundleActionCommand
	[[nodiscard]] int doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr) override;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_UninstallCommand_INCLUDED
