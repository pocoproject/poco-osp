//
// LoginCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  LoginCommand
//
// Definition of the LoginCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_LoginCommand_INCLUDED
#define OSP_Shell_LoginCommand_INCLUDED


#include "Poco/OSP/Shell/AbstractCommand.h"
#include "Poco/OSP/BundleContext.h"


namespace Poco::OSP::Shell {


class OSPShell_API LoginCommand: public AbstractCommand
	/// LoginCommand implements the "login" command,
	/// with the help of the Session object.
	///
	/// The login command takes two arguments, a user
	/// name and a password, and passes them to the
	/// login() member function of Session.
	///
	/// The arguments are not processed as options, also if
	/// they start with '-': only a single --help or -h argument
	/// shows the help, and a leading -- argument is skipped.
	///
	/// The help text of the login command can be
	/// customized by providing the following bundle
	/// properties:
	///    - login.description: The description of the command.
	///    - login.usage: The usage string.
{
public:
	LoginCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the LoginCommand.
		
	~LoginCommand() override;
		/// Destroys the LoginCommand.

	// Command
	[[nodiscard]] int execute(Session& session, const std::vector<std::string>& args, std::ostream& ostr) override;

protected:		
	// AbstractCommand
	[[nodiscard]] int executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr) override;
	
private:
	Poco::OSP::BundleContext::Ptr _pContext;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_LoginCommand_INCLUDED
