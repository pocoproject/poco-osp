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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
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
