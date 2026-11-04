//
// UninstallCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  UninstallCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "UninstallCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Bundle.h"
#include <ostream>


namespace Poco::OSP::Shell {


UninstallCommand::UninstallCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "uninstall")
{
}


UninstallCommand::~UninstallCommand() = default;


int UninstallCommand::doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr)
{
	if (isRequiredByShell(pBundle))
	{
		ostr << "Cannot uninstall bundle " << pBundle->symbolicName()
		     << " from a shell session: the shell requires it.\n";
		return Command::STATUS_ERROR;
	}
	pBundle->uninstall();
	return Command::STATUS_OK;
}


} // namespace Poco::OSP::Shell
