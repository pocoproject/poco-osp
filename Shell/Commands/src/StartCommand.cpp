//
// StartCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  StartCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "StartCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Bundle.h"
#include <ostream>


namespace Poco::OSP::Shell {


StartCommand::StartCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "start")
{
}


StartCommand::~StartCommand() = default;


int StartCommand::doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream&)
{
	pBundle->start();
	return Command::STATUS_OK;
}


} // namespace Poco::OSP::Shell
