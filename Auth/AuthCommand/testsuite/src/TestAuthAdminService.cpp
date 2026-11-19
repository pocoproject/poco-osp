//
// TestAuthAdminService.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestAuthAdminService.h"
#include "Poco/DateTime.h"
#include "Poco/Exception.h"
#include <map>
#include <set>
#include <string>
#include <vector>


using namespace std::string_literals;


const std::string TestAuthAdminService::ADMIN("admin");
const std::string TestAuthAdminService::ADMIN_PASSWORD("secret");
const std::string TestAuthAdminService::GUEST("guest");
const std::string TestAuthAdminService::GUEST_PASSWORD("guest");
const std::string TestAuthAdminService::ADMIN_PERMISSION("authAdmin");


TestAuthAdminService::TestAuthAdminService():
	_passwords{{ADMIN, ADMIN_PASSWORD}, {GUEST, GUEST_PASSWORD}},
	_permissions{{ADMIN, {ADMIN_PERMISSION}}}
{
}


bool TestAuthAdminService::authenticate(const std::string& userName, const std::string& credentials) const
{
	const auto it = _passwords.find(userName);
	return it != _passwords.end() && it->second == credentials;
}


bool TestAuthAdminService::authorize(const std::string& userName, const std::string& permission) const
{
	const auto it = _permissions.find(userName);
	return it != _permissions.end() && it->second.contains(permission);
}


bool TestAuthAdminService::authorize(const std::string& userName, const std::string&,
	const std::string& permission) const
{
	return authorize(userName, permission);
}


bool TestAuthAdminService::userExists(const std::string& userName) const
{
	return _passwords.contains(userName);
}


void TestAuthAdminService::changePassword(const std::string& username, const std::string& password)
{
	if (password.empty()) throw Poco::InvalidArgumentException("The password must not be empty"s, username);

	const auto it = _passwords.find(username);
	if (it == _passwords.end()) throw Poco::NotFoundException(username);
	it->second = password;
}


void TestAuthAdminService::addUser(const std::string& username, const std::string& password)
{
	if (password.empty()) throw Poco::InvalidArgumentException("The password must not be empty"s, username);

	if (!_passwords.emplace(username, password).second) throw Poco::ExistsException(username);
}


std::string TestAuthAdminService::getUserAttribute(const std::string& username, const std::string& attribute,
	const std::string& deflt) const
{
	const auto itUser = _attributes.find(username);
	if (itUser == _attributes.end()) return deflt;

	const auto itAttribute = itUser->second.find(attribute);
	return itAttribute != itUser->second.end() ? itAttribute->second : deflt;
}


void TestAuthAdminService::setUserAttribute(const std::string& username, const std::string& attribute,
	const std::string& value)
{
	if (isAuthAttribute(attribute)) throw Poco::InvalidArgumentException("Reserved attribute name"s, attribute);
	if (!userExists(username)) throw Poco::NotFoundException(username);

	_attributes[username][attribute] = value;
}


bool TestAuthAdminService::verifyCredentials(const std::string&, const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::permissionsForUser(const std::string& username, std::set<std::string>& permissions) const
{
	const auto it = _permissions.find(username);
	permissions = it != _permissions.end() ? it->second : std::set<std::string>();
}


void TestAuthAdminService::effectivePermissionsForUser(const std::string&, std::set<std::string>&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::grantPermissionsToUser(const std::string& username,
	const std::set<std::string>& permissions)
{
	if (!userExists(username)) throw Poco::NotFoundException(username);

	_permissions[username].insert(permissions.begin(), permissions.end());
}


void TestAuthAdminService::replacePermissionsForUser(const std::string&, const std::set<std::string>&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::revokePermissionsFromUser(const std::string& username,
	const std::set<std::string>& permissions)
{
	for (const auto& permission: permissions) _permissions[username].erase(permission);
}


int TestAuthAdminService::users(std::set<std::string>&, int, int) const
{
	throw Poco::NotImplementedException();
}


int TestAuthAdminService::usersWithName(std::set<std::string>&, const std::string&, int, int) const
{
	throw Poco::NotImplementedException();
}


int TestAuthAdminService::usersWithPermission(std::set<std::string>&, const std::string&, int, int) const
{
	throw Poco::NotImplementedException();
}


int TestAuthAdminService::usersWithRole(std::set<std::string>&, const std::string&, int, int) const
{
	throw Poco::NotImplementedException();
}


int TestAuthAdminService::usersWithAttribute(std::set<std::string>&, const std::string&, const std::string&, int,
	int) const
{
	throw Poco::NotImplementedException();
}


bool TestAuthAdminService::userEnabled(const std::string&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::removeUser(const std::string& username)
{
	_passwords.erase(username);
	_permissions.erase(username);
	_attributes.erase(username);
}


void TestAuthAdminService::permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const
{
	const auto it = _rolePermissions.find(rolename);
	permissions = it != _rolePermissions.end() ? it->second : std::set<std::string>();
}


void TestAuthAdminService::grantPermissionsToRole(const std::string& rolename,
	const std::set<std::string>& permissions)
{
	if (!roleExists(rolename)) throw Poco::NotFoundException(rolename);

	_rolePermissions[rolename].insert(permissions.begin(), permissions.end());
}


void TestAuthAdminService::replacePermissionsForRole(const std::string&, const std::set<std::string>&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::revokePermissionsFromRole(const std::string& rolename,
	const std::set<std::string>& permissions)
{
	for (const auto& permission: permissions) _rolePermissions[rolename].erase(permission);
}


bool TestAuthAdminService::roleExists(const std::string& rolename) const
{
	return _rolePermissions.contains(rolename);
}


void TestAuthAdminService::roles(std::set<std::string>&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::addRole(const std::string& rolename)
{
	if (!_rolePermissions.emplace(rolename, std::set<std::string>()).second) throw Poco::ExistsException(rolename);
}


void TestAuthAdminService::removeRole(const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::rolesForUser(const std::string&, std::set<std::string>&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::assignRoleToUser(const std::string&, const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::removeRoleFromUser(const std::string&, const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::replaceRolesForUser(const std::string&, const std::set<std::string>&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::uncacheUser(const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::removeUserAttribute(const std::string& username, const std::string& attribute)
{
	if (isAuthAttribute(attribute)) throw Poco::InvalidArgumentException("Reserved attribute name"s, attribute);

	_attributes[username].erase(attribute);
}


void TestAuthAdminService::setAuthAttribute(const std::string&, const std::string&, const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::removeAuthAttribute(const std::string&, const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::replaceUserAttributes(const std::string&, const std::map<std::string, std::string>&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::updateUserAttributes(const std::string&, const std::map<std::string, std::string>&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::attributesForUser(const std::string&, std::set<std::string>&) const
{
	throw Poco::NotImplementedException();
}


int TestAuthAdminService::attributesForUser(const std::string&, std::map<std::string, std::string>&) const
{
	throw Poco::NotImplementedException();
}


std::vector<std::string> TestAuthAdminService::findUsersByAttribute(const std::string&, const std::string&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::revokePermission(const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::lockOutUser(const std::string& username, const Poco::DateTime&)
{
	_lockedOutUsers.insert(username);
}


void TestAuthAdminService::unlockOutUser(const std::string& username)
{
	_lockedOutUsers.erase(username);
}


bool TestAuthAdminService::isUserLockedOut(const std::string& username) const
{
	return _lockedOutUsers.contains(username);
}


bool TestAuthAdminService::isExternalUser(const std::string&) const
{
	throw Poco::NotImplementedException();
}


bool TestAuthAdminService::isExternalRole(const std::string&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::allRoleMappings(std::map<std::string, std::string>&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::roleMappingForExternal(const std::string&, std::string&) const
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::setRoleMapping(const std::string&, const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::removeRoleMapping(const std::string&)
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::reset()
{
	throw Poco::NotImplementedException();
}


void TestAuthAdminService::invalidateCaches(int)
{
	throw Poco::NotImplementedException();
}
