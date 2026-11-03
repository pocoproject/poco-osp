//
// PooledAuthAdminService.cpp
//
// Library: OSP/Auth
// Package: Auth
// Module:  PooledAuthAdminService
//
// Copyright (c) 2023, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/PooledAuthAdminService.h"
#include "Poco/Delegate.h"
#include "Poco/Hash.h"
#include "Poco/String.h"
#include <cstddef>


namespace Poco::OSP::Auth {


PooledAuthAdminService::PooledAuthAdminService(const std::vector<AuthAdminService::Ptr>& backends):
	_backends(backends)
{
	poco_assert (!backends.empty());
	_random.seed();

	for (auto pBackend: _backends)
	{
		pBackend->userAdded += Poco::delegate(this, &PooledAuthAdminService::onUserAdded);
		pBackend->userRemoved += Poco::delegate(this, &PooledAuthAdminService::onUserRemoved);
		pBackend->roleAdded += Poco::delegate(this, &PooledAuthAdminService::onRoleAdded);
		pBackend->roleRemoved += Poco::delegate(this, &PooledAuthAdminService::onRoleRemoved);
		pBackend->validateUser += Poco::delegate(this, &PooledAuthAdminService::onValidateUser);
	}
}


PooledAuthAdminService::~PooledAuthAdminService()
{
	for (auto pBackend: _backends)
	{
		pBackend->userAdded -= Poco::delegate(this, &PooledAuthAdminService::onUserAdded);
		pBackend->userRemoved -= Poco::delegate(this, &PooledAuthAdminService::onUserRemoved);
		pBackend->roleAdded -= Poco::delegate(this, &PooledAuthAdminService::onRoleAdded);
		pBackend->roleRemoved -= Poco::delegate(this, &PooledAuthAdminService::onRoleRemoved);
		pBackend->validateUser -= Poco::delegate(this, &PooledAuthAdminService::onValidateUser);
	}
}


bool PooledAuthAdminService::verifyCredentials(const std::string& username, const std::string& credentials)
{
	return forUser(username)->verifyCredentials(username, credentials);
}


void PooledAuthAdminService::changePassword(const std::string& username, const std::string& password)
{
	forUser(username)->changePassword(username, password);
}


void PooledAuthAdminService::permissionsForUser(const std::string& username, std::set<std::string>& permissions) const
{
	forUser(username)->permissionsForUser(username, permissions);
}


void PooledAuthAdminService::effectivePermissionsForUser(const std::string& username, std::set<std::string>& permissions) const
{
	forUser(username)->effectivePermissionsForUser(username, permissions);
}


void PooledAuthAdminService::grantPermissionsToUser(const std::string& username, const std::set<std::string>& permissions)
{
	forUser(username)->grantPermissionsToUser(username, permissions);
}


void PooledAuthAdminService::replacePermissionsForUser(const std::string& username, const std::set<std::string>& permissions)
{
	forUser(username)->replacePermissionsForUser(username, permissions);
}


void PooledAuthAdminService::revokePermissionsFromUser(const std::string& username, const std::set<std::string>& permissions)
{
	forUser(username)->revokePermissionsFromUser(username, permissions);
}


int PooledAuthAdminService::users(std::set<std::string>& users, int first, int limit) const
{
	return any()->users(users, first, limit);
}


int PooledAuthAdminService::usersWithName(std::set<std::string>& users, const std::string& pattern, int first, int limit) const
{
	return any()->usersWithName(users, pattern, first, limit);
}


int PooledAuthAdminService::usersWithPermission(std::set<std::string>& users, const std::string& permission, int first, int limit) const
{
	return any()->usersWithPermission(users, permission, first, limit);
}


int PooledAuthAdminService::usersWithRole(std::set<std::string>& users, const std::string& role, int first, int limit) const
{
	return any()->usersWithRole(users, role, first, limit);
}


int PooledAuthAdminService::usersWithAttribute(std::set<std::string>& users, const std::string& attribute, const std::string& value, int first, int limit) const
{
	return any()->usersWithAttribute(users, attribute, value, first, limit);
}


void PooledAuthAdminService::addUser(const std::string& username, const std::string& password)
{
	forUser(username)->addUser(username, password);
}


void PooledAuthAdminService::removeUser(const std::string& username)
{
	forUser(username)->removeUser(username);
}


void PooledAuthAdminService::permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const
{
	any()->permissionsForRole(rolename, permissions);
}


void PooledAuthAdminService::grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions)
{
	any()->grantPermissionsToRole(rolename, permissions);
	invalidateCaches(INVALIDATE_CACHED_PERMISSIONS);
}


void PooledAuthAdminService::replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions)
{
	any()->replacePermissionsForRole(rolename, permissions);
	invalidateCaches(INVALIDATE_CACHED_PERMISSIONS);
}


void PooledAuthAdminService::revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions)
{
	any()->revokePermissionsFromRole(rolename, permissions);
	invalidateCaches(INVALIDATE_CACHED_PERMISSIONS);
}


bool PooledAuthAdminService::roleExists(const std::string& rolename) const
{
	return any()->roleExists(rolename);
}


void PooledAuthAdminService::roles(std::set<std::string>& roles) const
{
	any()->roles(roles);
}


void PooledAuthAdminService::addRole(const std::string& rolename)
{
	any()->addRole(rolename);
}


void PooledAuthAdminService::removeRole(const std::string& rolename)
{
	any()->removeRole(rolename);
	invalidateCaches(INVALIDATE_CACHED_PERMISSIONS);
}


void PooledAuthAdminService::rolesForUser(const std::string& username, std::set<std::string>& roles) const
{
	forUser(username)->rolesForUser(username, roles);
}


void PooledAuthAdminService::assignRoleToUser(const std::string& username, const std::string& rolename)
{
	forUser(username)->assignRoleToUser(username, rolename);
}


void PooledAuthAdminService::removeRoleFromUser(const std::string& username, const std::string& rolename)
{
	forUser(username)->removeRoleFromUser(username, rolename);
}


void PooledAuthAdminService::replaceRolesForUser(const std::string& username, const std::set<std::string>& permissions)
{
	forUser(username)->replaceRolesForUser(username, permissions);
}


void PooledAuthAdminService::uncacheUser(const std::string& username)
{
	forUser(username)->uncacheUser(username);
}


std::string PooledAuthAdminService::getUserAttribute(const std::string& username, const std::string& attribute, const std::string& deflt) const
{
	return forUser(username)->getUserAttribute(username, attribute, deflt);
}


void PooledAuthAdminService::setUserAttribute(const std::string& username, const std::string& attribute, const std::string& value)
{
	forUser(username)->setUserAttribute(username, attribute, value);
}


void PooledAuthAdminService::removeUserAttribute(const std::string& username, const std::string& attribute)
{
	forUser(username)->removeUserAttribute(username, attribute);
}


void PooledAuthAdminService::setAuthAttribute(const std::string& username, const std::string& attribute,
	const std::string& value)
{
	forUser(username)->setAuthAttribute(username, attribute, value);
}


void PooledAuthAdminService::removeAuthAttribute(const std::string& username, const std::string& attribute)
{
	forUser(username)->removeAuthAttribute(username, attribute);
}


void PooledAuthAdminService::replaceUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes)
{
	forUser(username)->replaceUserAttributes(username, attributes);
}


void PooledAuthAdminService::updateUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes)
{
	forUser(username)->updateUserAttributes(username, attributes);
}


void PooledAuthAdminService::attributesForUser(const std::string& username, std::set<std::string>& attributes) const
{
	forUser(username)->attributesForUser(username, attributes);
}


int PooledAuthAdminService::attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const
{
	return forUser(username)->attributesForUser(username, attributes);
}


std::vector<std::string> PooledAuthAdminService::findUsersByAttribute(const std::string& attribute, const std::string& value) const
{
	return any()->findUsersByAttribute(attribute, value);
}


void PooledAuthAdminService::revokePermission(const std::string& permission)
{
	any()->revokePermission(permission);
	invalidateCaches(INVALIDATE_CACHED_PERMISSIONS);
}


void PooledAuthAdminService::lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil)
{
	forUser(username)->lockOutUser(username, lockOutUntil);
}


void PooledAuthAdminService::unlockOutUser(const std::string& username)
{
	forUser(username)->unlockOutUser(username);
}


bool PooledAuthAdminService::isUserLockedOut(const std::string& username) const
{
	return forUser(username)->isUserLockedOut(username);
}


bool PooledAuthAdminService::isExternalUser(const std::string& username) const
{
	return forUser(username)->isExternalUser(username);
}


bool PooledAuthAdminService::isExternalRole(const std::string& rolename) const
{
	return forRole(rolename)->isExternalRole(rolename);
}


void PooledAuthAdminService::allRoleMappings(std::map<std::string, std::string>& mappings) const
{
	any()->allRoleMappings(mappings);
}


void PooledAuthAdminService::roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const
{
	any()->roleMappingForExternal(externalRole, internalRole);
}


void PooledAuthAdminService::setRoleMapping(const std::string& externalRole, const std::string& internalRole)
{
	any()->setRoleMapping(externalRole, internalRole);
	invalidateCaches(INVALIDATE_CACHED_PERMISSIONS);
}


void PooledAuthAdminService::removeRoleMapping(const std::string& externalRole)
{
	any()->removeRoleMapping(externalRole);
	invalidateCaches(INVALIDATE_CACHED_PERMISSIONS);
}


void PooledAuthAdminService::reset()
{
	for (auto pBackend: _backends)
	{
		pBackend->reset();
	}
}


void PooledAuthAdminService::invalidateCaches(int what)
{
	for (auto pBackend: _backends)
	{
		pBackend->invalidateCaches(what);
	}
}


bool PooledAuthAdminService::authenticate(const std::string& username, const std::string& credentials) const
{
	return forUser(username)->authenticate(username, credentials);
}


bool PooledAuthAdminService::authenticateAccount(const std::string& username, const std::string& credentials,
	std::string& accountName) const
{
	return forUser(username)->authenticateAccount(username, credentials, accountName);
}


bool PooledAuthAdminService::authorize(const std::string& username, const std::string& permission) const
{
	return forUser(username)->authorize(username, permission);
}


bool PooledAuthAdminService::authorize(const std::string& username, const std::string& roleOrScope, const std::string& permission) const
{
	return forUser(username)->authorize(username, roleOrScope, permission);
}


bool PooledAuthAdminService::userExists(const std::string& username) const
{
	return forUser(username)->userExists(username);
}


bool PooledAuthAdminService::userEnabled(const std::string& username) const
{
	return forUser(username)->userEnabled(username);
}


const std::type_info& PooledAuthAdminService::type() const
{
	return typeid(PooledAuthAdminService);
}


bool PooledAuthAdminService::isA(const std::type_info& otherType) const
{
	return otherType == typeid(PooledAuthAdminService) || AuthAdminService::isA(otherType);
}


AuthAdminService::Ptr PooledAuthAdminService::forUser(const std::string& username) const
{
	// A backend keeps an LDAP user under the lower case name.
	std::size_t index = Poco::hash(Poco::toLower(username)) % _backends.size();
	return _backends[index];
}


AuthAdminService::Ptr PooledAuthAdminService::forRole(const std::string& rolename) const
{
	std::size_t index = Poco::hash(rolename) % _backends.size();
	return _backends[index];
}


AuthAdminService::Ptr PooledAuthAdminService::any() const
{
	Poco::FastMutex::ScopedLock lock(_randomMutex);

	std::size_t index = _random.next(static_cast<Poco::UInt32>(_backends.size()));
	return _backends[index];
}


void PooledAuthAdminService::onUserAdded(const std::string& username)
{
	userAdded(username);
}


void PooledAuthAdminService::onUserRemoved(const std::string& username)
{
	userRemoved(username);
}


void PooledAuthAdminService::onRoleAdded(const std::string& rolename)
{
	roleAdded(rolename);
}


void PooledAuthAdminService::onRoleRemoved(const std::string& rolename)
{
	roleRemoved(rolename);
}


void PooledAuthAdminService::onValidateUser(ValidateUserEvent& event)
{
	validateUser(this, event);
}


} // namespace Poco::OSP::Auth
