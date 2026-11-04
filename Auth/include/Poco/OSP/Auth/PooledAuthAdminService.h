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
	/// backend. User names that differ only in case are
	/// forwarded to the same backend.
{
public:
	using Ptr = Poco::AutoPtr<PooledAuthAdminService>;

	explicit PooledAuthAdminService(const std::vector<AuthAdminService::Ptr>& backends);
		/// Creates the PooledAuthAdminService with the given backends.
		/// The vector must not be empty.

	~PooledAuthAdminService() override;
		/// Destroys the AuthAdminServiceImpl.

	// AuthAdminService
	[[nodiscard]] bool verifyCredentials(const std::string& username, const std::string& credentials) override;
	void changePassword(const std::string& username, const std::string& password) override;
	void permissionsForUser(const std::string& username, std::set<std::string>& permissions) const override;
	void effectivePermissionsForUser(const std::string& username, std::set<std::string>& permissions) const override;
	void grantPermissionsToUser(const std::string& username, const std::set<std::string>& permissions) override;
	void replacePermissionsForUser(const std::string& username, const std::set<std::string>& permissions) override;
	void revokePermissionsFromUser(const std::string& username, const std::set<std::string>& permissions) override;
	int users(std::set<std::string>& users, int first = 0, int limit = 0) const override;
	int usersWithName(std::set<std::string>& users, const std::string& pattern, int first = 0,
		int limit = 0) const override;
	int usersWithPermission(std::set<std::string>& users, const std::string& permission, int first = 0,
		int limit = 0) const override;
	int usersWithRole(std::set<std::string>& users, const std::string& role, int first = 0,
		int limit = 0) const override;
	int usersWithAttribute(std::set<std::string>& users, const std::string& attribute, const std::string& value,
		int first = 0, int limit = 0) const override;
	void addUser(const std::string& username, const std::string& password) override;
	void removeUser(const std::string& username) override;
	void permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const override;
	void grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	[[nodiscard]] bool roleExists(const std::string& rolename) const override;
	void roles(std::set<std::string>& roles) const override;
	void addRole(const std::string& rolename) override;
	void removeRole(const std::string& rolename) override;
	void rolesForUser(const std::string& username, std::set<std::string>& roles) const override;
	void assignRoleToUser(const std::string& username, const std::string& rolename) override;
	void removeRoleFromUser(const std::string& username, const std::string& rolename) override;
	void replaceRolesForUser(const std::string& username, const std::set<std::string>& permissions) override;
	void uncacheUser(const std::string& username) override;
	[[nodiscard]] std::string getUserAttribute(const std::string& username, const std::string& attribute,
		const std::string& deflt = std::string()) const override;
	void setUserAttribute(const std::string& username, const std::string& attribute, const std::string& value) override;
	void removeUserAttribute(const std::string& username, const std::string& attribute) override;
	void setAuthAttribute(const std::string& username, const std::string& attribute, const std::string& value) override;
	void removeAuthAttribute(const std::string& username, const std::string& attribute) override;
	void replaceUserAttributes(const std::string& username,
		const std::map<std::string, std::string>& attributes) override;
	void updateUserAttributes(const std::string& username,
		const std::map<std::string, std::string>& attributes) override;
	void attributesForUser(const std::string& username, std::set<std::string>& attributes) const override;
	int attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const override;
	[[nodiscard]] std::vector<std::string> findUsersByAttribute(const std::string& attribute,
		const std::string& value) const override;
	void revokePermission(const std::string& permission) override;
	void lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil) override;
	void unlockOutUser(const std::string& username) override;
	[[nodiscard]] bool isUserLockedOut(const std::string& username) const override;
	[[nodiscard]] bool isExternalUser(const std::string& username) const override;
	[[nodiscard]] bool isExternalRole(const std::string& rolename) const override;
	void allRoleMappings(std::map<std::string, std::string>& mappings) const override;
	void roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const override;
	void setRoleMapping(const std::string& externalRole, const std::string& internalRole) override;
	void removeRoleMapping(const std::string& externalRole) override;
	void reset() override;
	void invalidateCaches(int what = INVALIDATE_ALL) override;

	// AuthService
	[[nodiscard]] bool authenticate(const std::string& username, const std::string& credentials) const override;
	[[nodiscard]] bool authenticateAccount(const std::string& username, const std::string& credentials,
		std::string& accountName) const override;
		/// Forwards the login to the backend for username, which may differ
		/// from the backend for the account name that it returns; the result
		/// is the same, as the backends share the database. The names of a
		/// user, such as the account name and the user principal name, can
		/// be forwarded to different backends, and a backend serialises only
		/// its own logins of a user (see AuthAdminServiceImpl::acceptLDAPAccount()).
	[[nodiscard]] bool authorize(const std::string& username, const std::string& permission) const override;
	[[nodiscard]] bool authorize(const std::string& userName, const std::string& roleOrScope,
		const std::string& permission) const override;
	[[nodiscard]] bool userExists(const std::string& username) const override;
	[[nodiscard]] bool userEnabled(const std::string& username) const override;

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

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
