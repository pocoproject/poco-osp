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


void UninstallCommand::doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream&)
{
	pBundle->uninstall();
}


} } } // namespace Poco::OSP::Shell
