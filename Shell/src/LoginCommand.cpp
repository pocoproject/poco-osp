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
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/LoginCommand.h"
#include "Poco/OSP/Shell/Session.h"
#include <ostream>
#include <string>
#include <vector>


namespace Poco::OSP::Shell {


LoginCommand::LoginCommand(Poco::OSP::BundleContext::Ptr pContext):
	AbstractCommand(
		pContext->thisBundle()->properties().getString("login.description"),
		pContext->thisBundle()->properties().getString("login.usage")),
	_pContext(pContext)
{
}


LoginCommand::~LoginCommand() = default;


int LoginCommand::execute(Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	if (args.size() == 2 && (args.back() == "--help" || args.back() == "-h"))
		return AbstractCommand::execute(session, args, ostr);

	auto first = args.begin() + 1;
	if (first != args.end() && *first == "--") ++first;
	return executeImpl(session, std::vector<std::string>(first, args.end()), ostr);
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


} // namespace Poco::OSP::Shell
