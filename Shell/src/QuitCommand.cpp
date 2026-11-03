//
// QuitCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  QuitCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/QuitCommand.h"
#include "Poco/OSP/Shell/Session.h"


namespace Poco::OSP::Shell {


QuitCommand::QuitCommand(Poco::OSP::BundleContext::Ptr pContext):
	AbstractCommand(
		pContext->thisBundle()->properties().getString("quit.description"),
		pContext->thisBundle()->properties().getString("quit.usage")),
	_pContext(pContext)
{
}


QuitCommand::~QuitCommand() = default;


int QuitCommand::executeImpl(Session& /*session*/, const std::vector<std::string>& /*args*/, std::ostream& /*ostr*/)
{
	return -1;
}


} // namespace Poco::OSP::Shell
