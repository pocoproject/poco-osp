//
// ResolveCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  ResolveCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ResolveCommand.h"
#include "Poco/NumberFormatter.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Bundle.h"
#include <ostream>


namespace Poco::OSP::Shell {


ResolveCommand::ResolveCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "resolve")
{
}


ResolveCommand::~ResolveCommand() = default;


int ResolveCommand::doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream&)
{
	pBundle->resolve();
	return Command::STATUS_OK;
}


} // namespace Poco::OSP::Shell
