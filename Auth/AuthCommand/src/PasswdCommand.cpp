//
// PasswdCommand.cpp
//
// Library: OSP/Auth
// Package: Shell
// Module:  PasswdCommand
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "PasswdCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceFinder.h"
#include <ostream>
#include <string>
#include <vector>


PasswdCommand::PasswdCommand(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(pContext)
{
}


PasswdCommand::~PasswdCommand() = default;


int PasswdCommand::execute(Poco::OSP::Shell::Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc =
		Poco::OSP::ServiceFinder::find<Poco::OSP::Auth::AuthAdminService>(_pContext);

	if (pAuthAdminSvc != nullptr)
	{
		if (args.size() < 3 || args[1] == "--help" || args[1] == "-h")
		{
			help(ostr);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
		else
		{
			return passwd(session, pAuthAdminSvc, args, ostr);
		}
	}
	else return Poco::OSP::Shell::Command::STATUS_ERROR;
}


void PasswdCommand::help(std::ostream& ostr)
{
	ostr
		<< "usage: passwd <username> <password>\n"
		<< "Change the password of the given user.\n"
		<< "The user name is the account name of the user, which\n"
		<< "can differ from the name given at the login.\n"
		<< "Without the authAdmin permission, only the password\n"
		<< "of the current user can be changed.\n"
		<< std::endl;
}


int PasswdCommand::passwd(Poco::OSP::Shell::Session& session, Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc,
	const std::vector<std::string>& args, std::ostream& ostr)
{
	// The extension of the command has no permission, so it also runs in a session without a login.
	if (!session.authenticated()) return Poco::OSP::Shell::Command::STATUS_DENIED;

	if (args.size() == 3)
	{
		std::string username = args[1];
		std::string password = args[2];
		if (username == session.user() || session.authorize("authAdmin"))
		{
			pAuthAdminSvc->changePassword(username, password);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
		else return Poco::OSP::Shell::Command::STATUS_DENIED;
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}
