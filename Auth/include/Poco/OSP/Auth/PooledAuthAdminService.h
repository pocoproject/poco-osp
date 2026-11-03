//
// PooledAuthAdminService.h
//
// Library: OSP/Auth
// Package: Auth
// Module:  PooledAuthAdminService
//
// Definition of PooledAuthAdminService class.
//
// Copyright (c) 2023, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_PooledAuthAdminService_INCLUDED
#define OSP_Auth_PooledAuthAdminService_INCLUDED


#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/Random.h"
#include "Poco/Mutex.h"


namespace Poco {
namespace OSP {
namespace Auth {


class OSPAuth_API PooledAuthAdminService: public AuthAdminService
	/// This class implements the AuthAdminService interface
	/// in such a way that all methods are forwarded to one
	/// of multiple other AuthAdminService instances.
	///
	/// This can be used in systems with a large number of
	/// concurrent users to prevent a single AuthAdminService
	/// instance from becoming a bottleneck.
	///
	/// In order to improve caching behavior, requests for
	/// a specific user are always forwarded to the same
	/// backend.
{
public:
	using Ptr = Poco::AutoPtr<PooledAuthAdminService>;

	explicit PooledAuthAdminService(const std::vector<AuthAdminService::Ptr>& backends);
		/// Creates the PooledAuthAdminService with the given backends.
		/// The vector must not be empty.

	~PooledAuthAdminService();
		/// Destroys the AuthAdminServiceImpl.

	// AuthAdminService
	bool verifyCredentials(const std::string& username, const std::string& credentials);
	void changePassword(const std::string& username, const std::string& password);
	void permissionsForUser(const std::string& username, std::set<std::string>& permissions) const;
	void effectivePermissionsForUser(const std::string& username, std::set<std::string>& permissions) const;
	void grantPermissionsToUser(const std::string& username, const std::set<std::string>& permissions);
	void replacePermissionsForUser(const std::string& username, const std::set<std::string>& permissions);
	void revokePermissionsFromUser(const std::string& username, const std::set<std::string>& permissions);
	int users(std::set<std::string>& users, int first = 0, int limit = 0) const;
	int usersWithName(std::set<std::string>& users, const std::string& pattern, int first = 0, int limit = 0) const;
	int usersWithPermission(std::set<std::string>& users, const std::string& permission, int first = 0, int limit = 0) const;
	int usersWithRole(std::set<std::string>& users, const std::string& role, int first = 0, int limit = 0) const;
	int usersWithAttribute(std::set<std::string>& users, const std::string& attribute, const std::string& value, int first = 0, int limit = 0) const;
	void addUser(const std::string& username, const std::string& password);
	void removeUser(const std::string& username);
	void permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const;
	void grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions);
	void replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions);
	void revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions);
	bool roleExists(const std::string& rolename) const;
	void roles(std::set<std::string>& roles) const;
	void addRole(const std::string& rolename);
	void removeRole(const std::string& rolename);
	void rolesForUser(const std::string& username, std::set<std::string>& roles) const;
	void assignRoleToUser(const std::string& username, const std::string& rolename);
	void removeRoleFromUser(const std::string& username, const std::string& rolename);
	void replaceRolesForUser(const std::string& username, const std::set<std::string>& permissions);
	void uncacheUser(const std::string& username);
	std::string getUserAttribute(const std::string& username, const std::string& attribute, const std::string& deflt = std::string()) const;
	void setUserAttribute(const std::string& username, const std::string& attribute, const std::string& value);
	void removeUserAttribute(const std::string& username, const std::string& attribute);
	void replaceUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes);
	void updateUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes);
	void attributesForUser(const std::string& username, std::set<std::string>& attributes) const;
	int attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const;
	std::vector<std::string> findUsersByAttribute(const std::string& attribute, const std::string& value) const;
	void revokePermission(const std::string& permission);
	void lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil);
	void unlockOutUser(const std::string& username);
	bool isUserLockedOut(const std::string& username) const;
	bool isExternalUser(const std::string& username) const;
	bool isExternalRole(const std::string& rolename) const;
	void allRoleMappings(std::map<std::string, std::string>& mappings) const;
	void roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const;
	void setRoleMapping(const std::string& externalRole, const std::string& internalRole);
	void removeRoleMapping(const std::string& externalRole);
	void reset();
	void invalidateCaches(int what = INVALIDATE_ALL);

	// AuthService
	bool authenticate(const std::string& username, const std::string& credentials) const;
	bool authorize(const std::string& username, const std::string& permission) const;
	bool authorize(const std::string& userName, const std::string& roleOrScope, const std::string& permission) const;
	bool userExists(const std::string& username) const;
	bool userEnabled(const std::string& username) const;

	// Service
	const std::type_info& type() const;
	bool isA(const std::type_info& otherType) const;

protected:
	AuthAdminService::Ptr forUser(const std::string& username) const;
	AuthAdminService::Ptr forRole(const std::string& rolename) const;
	AuthAdminService::Ptr any() const;
	void onUserAdded(const std::string& username);
	void onUserRemoved(const std::string& username);
	void onRoleAdded(const std::string& rolename);
	void onRoleRemoved(const std::string& rolename);
	void onValidateUser(ValidateUserEvent& event);

private:
	const std::vector<AuthAdminService::Ptr> _backends;
	mutable Poco::Random _random;
	mutable Poco::FastMutex _randomMutex;
};


} } } // namespace Poco::OSP::Auth


#endif // OSP_Auth_PooledAuthAdminService_INCLUDED
