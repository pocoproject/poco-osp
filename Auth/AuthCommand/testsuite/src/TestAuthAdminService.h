//
// TestAuthAdminService.h
//
// Definition of the TestAuthAdminService class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestAuthAdminService_INCLUDED
#define TestAuthAdminService_INCLUDED


#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/AutoPtr.h"
#include "Poco/DateTime.h"
#include <map>
#include <set>
#include <string>
#include <vector>


class TestAuthAdminService: public Poco::OSP::Auth::AuthAdminService
	/// Keeps users, their passwords, permissions and attributes in memory.
	///
	/// Knows the user "admin" with the password "secret" and the permission
	/// "authAdmin", and the user "guest" with the password "guest" and no
	/// permission. Throws the exceptions of the OSP Auth service for an empty
	/// password, a known or unknown user and an "auth_" attribute name; roles
	/// have permissions only, and a lock-out only marks the user.
	///
	/// The other members of the AuthAdminService interface, which the commands
	/// do not call in the tests, throw a Poco::NotImplementedException.
{
public:
	using Ptr = Poco::AutoPtr<TestAuthAdminService>;

	TestAuthAdminService();
		/// Creates the TestAuthAdminService with the users "admin" and "guest".

	TestAuthAdminService(const TestAuthAdminService&) = delete;
	TestAuthAdminService(TestAuthAdminService&&) = delete;
	TestAuthAdminService& operator = (const TestAuthAdminService&) = delete;
	TestAuthAdminService& operator = (TestAuthAdminService&&) = delete;

	// AuthService
	bool authenticate(const std::string& userName, const std::string& credentials) const override;
	bool authorize(const std::string& userName, const std::string& permission) const override;
	bool authorize(const std::string& userName, const std::string& roleOrScope,
		const std::string& permission) const override;
	bool userExists(const std::string& userName) const override;

	// AuthAdminService
	void changePassword(const std::string& username, const std::string& password) override;
	void addUser(const std::string& username, const std::string& password) override;
	std::string getUserAttribute(const std::string& username, const std::string& attribute,
		const std::string& deflt) const override;
	void setUserAttribute(const std::string& username, const std::string& attribute,
		const std::string& value) override;

	bool verifyCredentials(const std::string& username, const std::string& credentials) override;
	void permissionsForUser(const std::string& username, std::set<std::string>& permissions) const override;
	void effectivePermissionsForUser(const std::string& username, std::set<std::string>& permissions) const override;
	void grantPermissionsToUser(const std::string& username, const std::set<std::string>& permissions) override;
	void replacePermissionsForUser(const std::string& username, const std::set<std::string>& permissions) override;
	void revokePermissionsFromUser(const std::string& username, const std::set<std::string>& permissions) override;
	int users(std::set<std::string>& users, int first, int limit) const override;
	int usersWithName(std::set<std::string>& users, const std::string& pattern, int first, int limit) const override;
	int usersWithPermission(std::set<std::string>& users, const std::string& permission, int first,
		int limit) const override;
	int usersWithRole(std::set<std::string>& users, const std::string& role, int first, int limit) const override;
	int usersWithAttribute(std::set<std::string>& users, const std::string& attribute, const std::string& value,
		int first, int limit) const override;
	bool userEnabled(const std::string& username) const override;
	void removeUser(const std::string& username) override;
	void permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const override;
	void grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	void revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions) override;
	bool roleExists(const std::string& rolename) const override;
	void roles(std::set<std::string>& roles) const override;
	void addRole(const std::string& rolename) override;
	void removeRole(const std::string& rolename) override;
	void rolesForUser(const std::string& username, std::set<std::string>& roles) const override;
	void assignRoleToUser(const std::string& username, const std::string& rolename) override;
	void removeRoleFromUser(const std::string& username, const std::string& rolename) override;
	void replaceRolesForUser(const std::string& username, const std::set<std::string>& roles) override;
	void uncacheUser(const std::string& username) override;
	void removeUserAttribute(const std::string& username, const std::string& attribute) override;
	void setAuthAttribute(const std::string& username, const std::string& attribute,
		const std::string& value) override;
	void removeAuthAttribute(const std::string& username, const std::string& attribute) override;
	void replaceUserAttributes(const std::string& username,
		const std::map<std::string, std::string>& attributes) override;
	void updateUserAttributes(const std::string& username,
		const std::map<std::string, std::string>& attributes) override;
	void attributesForUser(const std::string& username, std::set<std::string>& attributes) const override;
	int attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const override;
	std::vector<std::string> findUsersByAttribute(const std::string& attribute,
		const std::string& value) const override;
	void revokePermission(const std::string& permission) override;
	void lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil) override;
	void unlockOutUser(const std::string& username) override;
	bool isUserLockedOut(const std::string& username) const override;
	bool isExternalUser(const std::string& username) const override;
	bool isExternalRole(const std::string& rolename) const override;
	void allRoleMappings(std::map<std::string, std::string>& mappings) const override;
	void roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const override;
	void setRoleMapping(const std::string& externalRole, const std::string& internalRole) override;
	void removeRoleMapping(const std::string& externalRole) override;
	void reset() override;
	void invalidateCaches(int what) override;

	static const std::string ADMIN;
	static const std::string ADMIN_PASSWORD;
	static const std::string GUEST;
	static const std::string GUEST_PASSWORD;
	static const std::string ADMIN_PERMISSION;

protected:
	~TestAuthAdminService() override = default;
		/// Reference counted: destroyed through release() only.

private:
	std::map<std::string, std::string> _passwords;
	std::map<std::string, std::set<std::string>> _permissions;
	std::map<std::string, std::map<std::string, std::string>> _attributes;
	std::map<std::string, std::set<std::string>> _rolePermissions;
	std::set<std::string> _lockedOutUsers;
};


#endif // TestAuthAdminService_INCLUDED
