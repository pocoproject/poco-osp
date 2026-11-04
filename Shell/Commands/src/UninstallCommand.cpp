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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "UninstallCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Bundle.h"
#include <ostream>


namespace Poco {
namespace OSP {
namespace Shell {


UninstallCommand::UninstallCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "uninstall")
{
}


UninstallCommand::~UninstallCommand()
{
}


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


} } } // namespace Poco::OSP::Shell
