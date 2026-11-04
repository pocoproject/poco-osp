//
// LoginCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  LoginCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/LoginCommand.h"
#include "Poco/OSP/Shell/Session.h"


namespace Poco {
namespace OSP {
namespace Shell {


LoginCommand::LoginCommand(Poco::OSP::BundleContext::Ptr pContext):
	AbstractCommand(
		pContext->thisBundle()->properties().getString("login.description"),
		pContext->thisBundle()->properties().getString("login.usage")),
	_pContext(pContext)
{
}


LoginCommand::~LoginCommand()
{
}


int LoginCommand::executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	if (args.size() == 2)
	{
		if (session.login(args[0], args[1]))
			return Command::STATUS_OK;
		else
			return Command::STATUS_DENIED;
	}
	else 
	{
		ostr << "Username and password required." << '\n';
		return Command::STATUS_USAGE;
	}
}


} } } // namespace Poco::OSP::Shell
