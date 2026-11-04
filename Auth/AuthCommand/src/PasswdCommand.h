//
// PasswdCommand.h
//
// Library: OSP/Auth
// Package: Shell
// Module:  PasswdCommand
//
// Definition of the PasswdCommand class.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_PasswdCommand_INCLUDED
#define OSP_Auth_PasswdCommand_INCLUDED


#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/BundleContext.h"
#include <ostream>
#include <string>
#include <vector>


class PasswdCommand: public Poco::OSP::Shell::Command
	/// This class implements the "passwd" command.
{
public:
	explicit PasswdCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the PasswdCommand, using the given bundle context
		/// to find the AuthAdminService.

	PasswdCommand() = delete;

	~PasswdCommand() override;
		/// Destroys the PasswdCommand.

	// Command
	int execute(Poco::OSP::Shell::Session& session, const std::vector<std::string>& args, std::ostream& ostr) override;

protected:
	void help(std::ostream& ostr);
		/// Writes the usage of the command to ostr.

	int passwd(Poco::OSP::Shell::Session& session, Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc,
		const std::vector<std::string>& args, std::ostream& ostr);
		/// Changes the password of the user named in args. Returns STATUS_DENIED
		/// in a session without a login, and for another user than the session's
		/// user without the authAdmin permission.

private:
	Poco::OSP::BundleContext::Ptr _pContext;
};


#endif // OSP_Auth_PasswdCommand_INCLUDED
