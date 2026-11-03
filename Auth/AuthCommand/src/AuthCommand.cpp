//
// AuthCommand.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "AuthCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/DateTime.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/Exception.h"
#include "Poco/Glob.h"
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>


using namespace std::string_literals;


AuthCommand::AuthCommand(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(pContext)
{
}


AuthCommand::~AuthCommand() = default;


int AuthCommand::execute(Poco::OSP::Shell::Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc =
		Poco::OSP::ServiceFinder::find<Poco::OSP::Auth::AuthAdminService>(_pContext);

	if (pAuthAdminSvc != nullptr)
	{
		if (session.authorize("authAdmin"s))
		{
			if (args.size() == 1 || args[1] == "help" || args[1] == "--help" || args[1] == "-h")
			{
				help(ostr);
				return Poco::OSP::Shell::Command::STATUS_OK;
			}
			else if (args.size() > 1)
			{
				if (args[1] == "show")
				{
					return show(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "grant")
				{
					return grant(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "revoke")
				{
					return revoke(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "assign")
				{
					return assign(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "unassign")
				{
					return unassign(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "add")
				{
					return add(pAuthAdminSvc, args, ostr, session.user());
				}
				else if (args[1] == "remove" || args[1] == "rm")
				{
					return remove(pAuthAdminSvc, args, ostr, session.user());
				}
				else if (args[1] == "passwd")
				{
					return passwd(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "set")
				{
					return set(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "unset")
				{
					return unset(pAuthAdminSvc, args, ostr);
				}
				else if (args[1] == "unlock")
				{
					return unlock(pAuthAdminSvc, args, ostr);
				}
			}
			help(ostr);
			return Poco::OSP::Shell::Command::STATUS_USAGE;
		}
		else  return Poco::OSP::Shell::Command::STATUS_DENIED;
	}
	else return Poco::OSP::Shell::Command::STATUS_ERROR;
}


void AuthCommand::help(std::ostream& ostr)
{
	ostr
		<< "usage: auth <command> <args>...\n"
		<< "User authentication and authorization administration.\n"
		<< "The following sub-commands are available:\n"
		<< "  show user <name> [roles|permissions|attributes [<attribute>...]]\n"
		<< "    Display information about a specific user.\n"
		<< "  show role <name>\n"
		<< "    Display information about a specific role.\n"
		<< "  show users [<pattern> | role:<role> | permission:<permission> | attribute:<name>=<value>]\n"
		<< "    Display all available users, or those matching <pattern> or condition.\n"
		<< "    Pattern can contain placeholders '*' for zero or more characters and '?' for single character.\n"
		<< "    Permission and attribute options can be abbreviated, e.g. perm:<permission> or attr:<name>=<value>.\n"
		<< "  show roles [<pattern>]\n"
		<< "    Display all available roles, or those matching <pattern>.\n"
		<< "  grant user|role <name> <permission>...\n"
		<< "    Grant a permission to the user or role with the given name.\n"
		<< "  revoke user|role <name> <permission>...\n"
		<< "    Revoke a permission from the user or role with the given name.\n"
		<< "  assign <username> <rolename>\n"
		<< "    Assign a role to a user.\n"
		<< "  unassign <username> <rolename>\n"
		<< "    Remove a role from a user.\n"
		<< "  add user <name> <password>\n"
		<< "  add role <name>\n"
		<< "    Add the specified user or role.\n"
		<< "  remove user|role <name>\n"
		<< "  rm user|role <name>\n"
		<< "    Remove the specified user or role.\n"
		<< "  passwd <username> <password>\n"
		<< "    Change the password for the given user.\n"
		<< "  set user <name> <attribute>=<value>...\n"
		<< "    Sets the given attributes of a user.\n"
		<< "  unset user <name> <attribute>...\n"
		<< "    Removes the given attributes of a user.\n"
		<< "  unlock <username>\n"
		<< "    Unlocks a user after it has bee locked out due to too many failed login attempts.\n"
		<< "  help\n"
		<< "    Display this help screen.\n"
		<< std::endl;
}


int AuthCommand::show(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() >= 4 && args[2] == "user")
	{
		if (args.size() > 4 && args[4] == "roles")
			return showUserRoles(pAuthAdminSvc, args[3], ostr);
		else if (args.size() > 4 && args[4] == "permissions")
			return showUserPermissions(pAuthAdminSvc, args[3], ostr);
		else if (args.size() > 4 && args[4] == "attributes")
			return showUserAttributes(pAuthAdminSvc, args, ostr);
		else
			return showUser(pAuthAdminSvc, args[3], ostr);
	}
	else if (args.size() == 4 && args[2] == "role")
	{
		return showRole(pAuthAdminSvc, args[3], ostr);
	}
	else if (args.size() == 3 && args[2] == "users")
	{
		return showUsers(pAuthAdminSvc, ostr, "*");
	}
	else if (args.size() > 3 && args[2] == "users")
	{
		return showUsers(pAuthAdminSvc, ostr, args[3]);
	}
	else if (args.size() == 3 && args[2] == "roles")
	{
		return showRoles(pAuthAdminSvc, ostr, "*");
	}
	else if (args.size() > 3 && args[2] == "roles")
	{
		return showRoles(pAuthAdminSvc, ostr, args[3]);
	}
	else
	{
		ostr << "Bad command syntax." << std::endl;
		return Poco::OSP::Shell::Command::STATUS_USAGE;
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::showUser(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& userName,
	std::ostream& ostr)
{
	std::set<std::string> roles;
	pAuthAdminSvc->rolesForUser(userName, roles);
	std::set<std::string> permissions;
	pAuthAdminSvc->effectivePermissionsForUser(userName, permissions);
	std::map<std::string, std::string> attributes;
	pAuthAdminSvc->attributesForUser(userName, attributes);
	for (std::set<std::string>::const_iterator it = roles.begin(); it != roles.end(); ++it)
	{
		ostr << "role: " << *it << std::endl;
	}
	for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
	{
		ostr << "permission: " << *it << std::endl;
	}
	for (std::map<std::string, std::string>::const_iterator it = attributes.begin(); it != attributes.end(); ++it)
	{
		ostr << "attribute: " << it->first << "=" << it->second << std::endl;
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::showUserRoles(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& userName,
	std::ostream& ostr)
{
	std::set<std::string> roles;
	pAuthAdminSvc->rolesForUser(userName, roles);
	for (std::set<std::string>::const_iterator it = roles.begin(); it != roles.end(); ++it)
	{
		ostr << *it << std::endl;
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::showUserPermissions(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& userName,
	std::ostream& ostr)
{
	std::set<std::string> permissions;
	pAuthAdminSvc->effectivePermissionsForUser(userName, permissions);
	for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
	{
		ostr << *it << std::endl;
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::showUserAttributes(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc,
	const std::vector<std::string>& args, std::ostream& ostr)
{
	std::set<std::string> attributes;
	if (args.size() > 5)
	{
		for (std::vector<std::string>::size_type i = 5; i < args.size(); i++)
		{
			attributes.insert(args[i]);
		}
	}
	else
	{
		pAuthAdminSvc->attributesForUser(args[3], attributes);
	}
	for (std::set<std::string>::const_iterator it = attributes.begin(); it != attributes.end(); ++it)
	{
		ostr << *it << "=" << pAuthAdminSvc->getUserAttribute(args[3], *it, "<undefined>") << std::endl;
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::showRole(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::string& roleName,
	std::ostream& ostr)
{
	std::set<std::string> permissions;
	pAuthAdminSvc->permissionsForRole(roleName, permissions);
	for (std::set<std::string>::const_iterator it = permissions.begin(); it != permissions.end(); ++it)
	{
		ostr << "permission: " << *it << std::endl;
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::showUsers(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, std::ostream& ostr,
	const std::string& pattern)
{
	std::set<std::string> users;
	if (pattern.compare(0, 5, "role:") == 0)
	{
		std::string role(pattern, 5);
		pAuthAdminSvc->usersWithRole(users, role);
	}
	else if (pattern.compare(0, 11, "permission:") == 0 || pattern.compare(0, 5, "perm:") == 0)
	{
		std::string::size_type pos = pattern.find(':');
		std::string permission(pattern, pos + 1);
		pAuthAdminSvc->usersWithPermission(users, permission);
	}
	else if (pattern.compare(0, 10, "attribute:") == 0 || pattern.compare(0, 5, "attr:") == 0)
	{
		std::string::size_type pos = pattern.find(':');
		std::string attribute(pattern, pos + 1);
		pos = attribute.find('=');
		if (pos != std::string::npos)
		{
			std::string name(attribute, 0, pos);
			std::string value(attribute, pos + 1);
			pAuthAdminSvc->usersWithAttribute(users, name, value);
		}
		else throw Poco::InvalidArgumentException("Invalid attribute specification", attribute);
	}
	else if (!pattern.empty())
	{
		pAuthAdminSvc->usersWithName(users, pattern);
	}
	else
	{
		pAuthAdminSvc->users(users);
	}
	for (std::set<std::string>::const_iterator it = users.begin(); it != users.end(); ++it)
	{
		ostr << *it << std::endl;
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::showRoles(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, std::ostream& ostr,
	const std::string& pattern)
{
	Poco::Glob glob(pattern);
	std::set<std::string> roles;
	pAuthAdminSvc->roles(roles);
	for (std::set<std::string>::const_iterator it = roles.begin(); it != roles.end(); ++it)
	{
		if (glob.match(*it))
		{
			ostr << *it << std::endl;
		}
	}
	return Poco::OSP::Shell::Command::STATUS_OK;
}


int AuthCommand::grant(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() > 4)
	{
		std::set<std::string> perms(args.begin() + 4, args.end());
		if (args[2] == "user")
		{
			pAuthAdminSvc->grantPermissionsToUser(args[3], perms);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
		else if (args[2] == "role")
		{
			pAuthAdminSvc->grantPermissionsToRole(args[3], perms);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::revoke(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() > 4)
	{
		std::set<std::string> perms(args.begin() + 4, args.end());
		if (args[2] == "user")
		{
			pAuthAdminSvc->revokePermissionsFromUser(args[3], perms);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
		else if (args[2] == "role")
		{
			pAuthAdminSvc->revokePermissionsFromRole(args[3], perms);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::assign(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() == 4)
	{
		pAuthAdminSvc->assignRoleToUser(args[2], args[3]);
		return Poco::OSP::Shell::Command::STATUS_OK;
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::unassign(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() == 4)
	{
		pAuthAdminSvc->removeRoleFromUser(args[2], args[3]);
		return Poco::OSP::Shell::Command::STATUS_OK;
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::add(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr, const std::string& user)
{
	if (args.size() > 3)
	{
		if (args[2] == "user" && args.size() == 5)
		{
			pAuthAdminSvc->addUser(args[3], args[4]);
			pAuthAdminSvc->setUserAttribute(args[3], "createdBy"s, user);
			pAuthAdminSvc->setUserAttribute(args[3], "created"s, Poco::DateTimeFormatter::format(Poco::DateTime(),
				Poco::DateTimeFormat::ISO8601_FORMAT));
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
		else if (args[2] == "role" && args.size() == 4)
		{
			pAuthAdminSvc->addRole(args[3]);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::remove(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr, const std::string& user)
{
	if (args.size() > 3)
	{
		if (args[2] == "user")
		{
			if (args[3] != user)
			{
				pAuthAdminSvc->removeUser(args[3]);
				return Poco::OSP::Shell::Command::STATUS_OK;
			}
			else
			{
				ostr << "Cannot delete yourself." << std::endl;
				return Poco::OSP::Shell::Command::STATUS_DENIED;
			}
		}
		else if (args[2] == "role")
		{
			pAuthAdminSvc->removeRole(args[3]);
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::passwd(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() == 4)
	{
		pAuthAdminSvc->changePassword(args[2], args[3]);
		return Poco::OSP::Shell::Command::STATUS_OK;
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::set(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() > 3)
	{
		if (args[2] == "user")
		{
			std::string username = args[3];
			for (std::vector<std::string>::size_type i = 4; i < args.size(); i++)
			{
				std::string attr;
				std::string value;
				std::string::size_type pos = args[i].find('=');
				if (pos != std::string::npos)
				{
					attr.assign(args[i], 0, pos);
					value.assign(args[i], pos + 1, args[i].size() - pos - 1);
				}
				else attr = args[i];
				pAuthAdminSvc->setUserAttribute(username, attr, value);
			}
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::unset(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() > 3)
	{
		if (args[2] == "user")
		{
			std::string username = args[3];
			for (std::vector<std::string>::size_type i = 4; i < args.size(); i++)
			{
				pAuthAdminSvc->removeUserAttribute(username, args[i]);
			}
			return Poco::OSP::Shell::Command::STATUS_OK;
		}
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}


int AuthCommand::unlock(Poco::OSP::Auth::AuthAdminService::Ptr pAuthAdminSvc, const std::vector<std::string>& args,
	std::ostream& ostr)
{
	if (args.size() == 3)
	{
		pAuthAdminSvc->unlockOutUser(args[2]);
		return Poco::OSP::Shell::Command::STATUS_OK;
	}
	ostr << "Bad command syntax." << std::endl;
	return Poco::OSP::Shell::Command::STATUS_USAGE;
}
