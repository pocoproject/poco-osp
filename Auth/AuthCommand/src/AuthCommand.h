//
// AuthCommand.h
//
// Library: OSP/Auth
// Package: Shell
// Module:  AuthCommand
//
// Definition of the AuthCommand class.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_AuthCommand_INCLUDED
#define OSP_Auth_AuthCommand_INCLUDED


#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/BundleContext.h"
#include <ostream>
#include <string>
#include <vector>


class AuthCommand: public Poco::OSP::Shell::Command
	/// This class implements the "auth" command
	/// for administrating users, roles and permissions.
{
public:
	explicit AuthCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the AuthCommand, using the given bundle context
		/// to find the AuthAdminService.

	AuthCommand() = delete;

	~AuthCommand() override;
		/// Destroys the AuthCommand.

	// Command
	int execute(Poco::OSP::Shell::Session& session, const std::vector<std::string>& args, std::ostream& ostr) override;

protected:
	void help(std::ostream& ostr);
		/// Writes the usage of the command and its sub-commands to ostr.

	int show(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth show": writes information about a user or a role, or
		/// the users or roles that match a pattern, to ostr.

	int showUser(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& userName,
		std::ostream& ostr);
		/// Writes the roles, the effective permissions and the attributes of the
		/// given user to ostr.

	int showUserRoles(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& userName,
		std::ostream& ostr);
		/// Writes the roles assigned to the given user to ostr.

	int showUserPermissions(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& userName,
		std::ostream& ostr);
		/// Writes the effective permissions of the given user, including those
		/// of its roles, to ostr.

	int showUserAttributes(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc,
		const std::vector<std::string>& args, std::ostream& ostr);
		/// Writes the attributes named in args, or all attributes, of the user
		/// named in args to ostr.

	int showRole(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& roleName,
		std::ostream& ostr);
		/// Writes the permissions of the given role to ostr.

	int showUsers(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, std::ostream& ostr,
		const std::string& pattern);
		/// Writes the names of the users that match the given pattern, role,
		/// permission or attribute condition to ostr.
		///
		/// Throws a Poco::InvalidArgumentException if an attribute condition
		/// has no value.

	int showRoles(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, std::ostream& ostr,
		const std::string& pattern);
		/// Writes the names of the roles that match the given pattern to ostr.

	int grant(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth grant": grants permissions to a user or role.

	int revoke(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth revoke": revokes permissions from a user or role.

	int assign(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth assign": assigns a role to a user.

	int unassign(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth unassign": removes a role from a user.

	int add(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr, const std::string& user);
		/// Executes "auth add": adds a user or a role. The given user, the user
		/// of the session, is stored in the createdBy attribute of a new user.

	int remove(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr, const std::string& user);
		/// Executes "auth remove": removes a user or a role. Returns STATUS_DENIED
		/// if the user to remove is the given user, the user of the session.

	int passwd(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth passwd": changes the password of a user.

	int set(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth set": sets attributes of a user.

	int unset(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth unset": removes attributes of a user.

	int unlock(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
		std::ostream& ostr);
		/// Executes "auth unlock": unlocks a user that was locked out after
		/// too many failed login attempts.

private:
	Poco::OSP::BundleContext::Ptr _pContext;
};


#endif // OSP_Auth_AuthCommand_INCLUDED
