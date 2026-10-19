//
// AuthAdminService.h
//
// Library: OSP/Auth
// Package: Auth
// Module:  AuthAdminService
//
// Definition of the AuthAdminService service interface class.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_AuthAdminService_INCLUDED
#define OSP_Auth_AuthAdminService_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/DateTime.h"
#include "Poco/BasicEvent.h"
#include "Poco/AutoPtr.h"
#include <set>
#include <map>


namespace Poco {
namespace OSP {
namespace Auth {

using namespace std::string_literals;

class OSPAuth_API AuthAdminService: public Poco::OSP::Auth::AuthService
	/// This service interface extends the basic AuthService
	/// interface with methods for administrating
	/// users, roles and their permissions.
	///
	/// Users can have assigned permissions and roles.
	/// Roles have assigned permissions as well. The effective
	/// permissions of a user are his directly assigned permissions
	/// plus the permissions assigned through the roles has has
	/// been assigned. Use of roles is thus optional.
	///
	/// Each user can have an arbitrary number of attributes.
{
public:
	using Ptr = Poco::AutoPtr<AuthAdminService>;

	struct ValidateUserEvent
	{
		std::string username;
			/// User name of user to validate.

		std::map<std::string, std::string> attributes;
			/// Attributes of user to validate.

		bool valid;
			/// True if the account is valid, otherwise false.
			///
			/// Will be set to true when the event is fired.
			/// Event handler can set to false to disable
			/// the user account. In this case, authenticate()
			/// and userEnabled() will return false.
	};

	Poco::BasicEvent<const std::string> userAdded;
		/// Fired when a new user account has been created.

	Poco::BasicEvent<const std::string> userRemoved;
		/// Fired when a user account has been removed.

	Poco::BasicEvent<const std::string> roleAdded;
		/// Fired when a new role has been created.

	Poco::BasicEvent<const std::string> roleRemoved;
		/// Fired when a role has been removed.

	Poco::BasicEvent<ValidateUserEvent> validateUser;
		/// Fired during authenticate() or userEnabled()
		/// to perform application-specific validation of
		/// a user account. This can be used to check if
		/// a user account has expired or otherwise
		/// disabled at the application level.

	enum
	{
		INVALIDATE_CACHED_PASSWORDS = 1,
		INVALIDATE_CACHED_PERMISSIONS = 2,
		INVALIDATE_ALL = INVALIDATE_CACHED_PASSWORDS | INVALIDATE_CACHED_PASSWORDS
	};

	virtual void changePassword(const std::string& username, const std::string& password) = 0;
		/// Changes the password for the user with the given username.

	virtual void permissionsForUser(const std::string& username, std::set<std::string>& permissions) const = 0;
		/// Returns a set of permissions for the user given by username.
		/// Only permissions directly granted to the user will be returned.
		/// Permissions granted indirectly through roles will not be included.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.

	virtual void effectivePermissionsForUser(const std::string& username, std::set<std::string>& permissions) const = 0;
		/// Returns a set of permissions for the user given by username.
		/// Both permissions directly granted to the user and
		/// permissions granted indirectly through roles will be included
		/// in the result.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.

	virtual void grantPermissionsToUser(const std::string& username, const std::set<std::string>& permissions) = 0;
		/// Grants the given permissions to the user identified by username.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.

	virtual void replacePermissionsForUser(const std::string& username, const std::set<std::string>& permissions) = 0;
		/// Removes all previously granted permissions and
		/// grants the given permissions to the user identified by username.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.

	virtual void revokePermissionsFromUser(const std::string& username, const std::set<std::string>& permissions) = 0;
		/// Revokes the given permissions from the user identified by username.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.

	virtual int users(std::set<std::string>& users, int first = 0, int limit = 0) const = 0;
		/// Returns a list of all users in the database.
		///
		/// This method supports "paged" retrieval. First specifies the
		/// index of the first user to be returned. Limit specifies the
		/// maximum number of users to return.
		///
		/// Returns the total number of users.

	virtual int usersWithName(std::set<std::string>& users, const std::string& pattern, int first = 0, int limit = 0) const = 0;
		/// Returns a list of all users in the database matching the given pattern.
		///
		/// An asterisk '*' in the expression matches any sequence of characters, even an empty sequence.
		/// A question mark '?' matches any single character.
		///
		/// This method supports "paged" retrieval. First specifies the
		/// index of the first user to be returned. Limit specifies the
		/// maximum number of users to return.
		///
		/// Returns the total number of users.

	virtual int usersWithPermission(std::set<std::string>& users, const std::string& permission, int first = 0, int limit = 0) const = 0;
		/// Returns a list of all users in the database having the given permission.
		///
		/// Only users having the given permission granted explicitly are returned.
		/// Users having a permission only granted implicitly via a role are ignored.
		///
		/// This method supports "paged" retrieval. First specifies the
		/// index of the first user to be returned. Limit specifies the
		/// maximum number of users to return.
		///
		/// Returns the total number of users with the given permission.

	virtual int usersWithRole(std::set<std::string>& users, const std::string& role, int first = 0, int limit = 0) const = 0;
		/// Returns a list of all users in the database having the given role.
		///
		/// Only users having been explicitly assigned the role are returned.
		///
		/// This method supports "paged" retrieval. First specifies the
		/// index of the first user to be returned. Limit specifies the
		/// maximum number of users to return.
		///
		/// Returns the total number of users with the given role.

	virtual int usersWithAttribute(std::set<std::string>& users, const std::string& attribute, const std::string& value, int first = 0, int limit = 0) const = 0;
		/// Returns a list of all users in the database having the given attribute.
		///
		/// This method supports "paged" retrieval. First specifies the
		/// index of the first user to be returned. Limit specifies the
		/// maximum number of users to return.
		///
		/// Returns the total number of users with the given attribute.

	virtual bool userEnabled(const std::string& username) const = 0;
		/// Returns true if the user with the given username is enabled.
		///
		/// If the implementation has no concept of disabling user accounts,
		/// this should just return true.

	virtual void addUser(const std::string& username, const std::string& password) = 0;
		/// Adds a new user to the database.
		///
		/// Throws a Poco::ExistsException if an user with the given username
		/// already exists.

	virtual void removeUser(const std::string& username) = 0;
		/// Removes the user identified by username from the database.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.

	virtual void permissionsForRole(const std::string& rolename, std::set<std::string>& permissions) const = 0;
		/// Returns a set of permissions for the role given by rolename.
		///
		/// Throws a Poco::NotFoundException if the role does not exist.

	virtual void grantPermissionsToRole(const std::string& rolename, const std::set<std::string>& permissions) = 0;
		/// Grants the given permissions to the role identified by rolename.
		///
		/// Throws a Poco::NotFoundException if the role does not exist.

	virtual void replacePermissionsForRole(const std::string& rolename, const std::set<std::string>& permissions) = 0;
		/// Removes all previously granted permissions and
		/// grants the given permissions to the role identified by rolename.
		///
		/// Throws a Poco::NotFoundException if the role does not exist.

	virtual void revokePermissionsFromRole(const std::string& rolename, const std::set<std::string>& permissions) = 0;
		/// Revokes the given permissions from the role identified by rolename.
		///
		/// Throws a Poco::NotFoundException if the role does not exist.

	virtual bool roleExists(const std::string& rolename) const = 0;
		/// Returns true iff a role with the given rolename exists.

	virtual void roles(std::set<std::string>& roles) const = 0;
		/// Returns a set of all roles in the database.

	virtual void addRole(const std::string& rolename) = 0;
		/// Adds a new role to the database.
		///
		/// Throws a Poco::ExistsException if a role with the given name
		/// already exists.

	virtual void removeRole(const std::string& rolename) = 0;
		/// Removes the role identified by name from the database.
		///
		/// Throws a Poco::NotFoundException if the role does not exist.

	virtual void rolesForUser(const std::string& username, std::set<std::string>& roles) const = 0;
		/// Returns the set of roles assigned to the user identified by username.

	virtual void assignRoleToUser(const std::string& username, const std::string& rolename) = 0;
		/// Assigns a role to an user.
		///
		/// Throws a Poco::NotFoundException if the user or role does not exist.
		/// Does nothing if the user already has the role.

	virtual void removeRoleFromUser(const std::string& username, const std::string& rolename) = 0;
		/// Removes a role from an user.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.
		/// Does nothing if the user does not have the role.

	virtual void replaceRolesForUser(const std::string& username, const std::set<std::string>& roles) = 0;
		/// Unassignes all roles from the user, then assigns the
		/// given roles.
		///
		/// Throws a Poco::NotFoundException if the user does not exist.

	virtual void uncacheUser(const std::string& username) = 0;
		/// Removes all cached data for the user with the given name.
		///
		/// Should be done when a user logs out of an application.

	virtual std::string getUserAttribute(const std::string& username, const std::string& attribute, const std::string& deflt = ""s) const = 0;
		/// Returns the value of a user attribute, or the specified default
		/// value if the user attribute does not exist.

	virtual void setUserAttribute(const std::string& username, const std::string& attribute, const std::string& value) = 0;
		/// Sets the value of a user attribute.

	virtual void removeUserAttribute(const std::string& username, const std::string& attribute) = 0;
		/// Removes the given attribute from username.

	virtual void replaceUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes) = 0;
		/// Replaces the attributes of the given user.
		/// Will also remove all other attributes.

	virtual void updateUserAttributes(const std::string& username, const std::map<std::string, std::string>& attributes) = 0;
		/// Updates the attributes of the given user.

	virtual void attributesForUser(const std::string& username, std::set<std::string>& attributes) const = 0;
		/// Returns the names of all attributes set for the given username.

	virtual void attributesForUser(const std::string& username, std::map<std::string, std::string>& attributes) const = 0;
		/// Returns the names and values of all attributes set for the given username.

	virtual std::vector<std::string> findUsersByAttribute(const std::string& attribute, const std::string& value) const = 0;
		/// Returns the names of all users having an attribute with the given value.

	virtual void revokePermission(const std::string& permission) = 0;
		/// Revokes the given permission from all users and roles.

	virtual void lockOutUser(const std::string& username, const Poco::DateTime& lockOutUntil) = 0;
		/// Locks out the given username until the given date/time.

	virtual void unlockOutUser(const std::string& username) = 0;
		/// Unlocks out the given username.

	virtual bool isUserLockedOut(const std::string& username) const = 0;
		/// Returns true if the given username has been locked out, otherwise false.

	virtual bool isExternalUser(const std::string& username) const = 0;
		/// Returns true if the given username is managed externally, e.g. by and LDAP server.

	virtual bool isExternalRole(const std::string& rolename) const = 0;
		/// Returns true if the given rolename is managed externally, e.g. by and LDAP server.

	virtual void allRoleMappings(std::map<std::string, std::string>& mappings) const = 0;
		/// Returns all role mappings (external role -> internal role).

	virtual void roleMappingForExternal(const std::string& externalRole, std::string& internalRole) const = 0;
		/// Returns the internal role mapped to the given external role.
		/// Sets internalRole to empty string if no mapping exists.

	virtual void setRoleMapping(const std::string& externalRole, const std::string& internalRole) = 0;
		/// Sets the mapping from external role to internal role.

	virtual void removeRoleMapping(const std::string& externalRole) = 0;
		/// Removes the mapping for the given external role.

	virtual void reset() = 0;
		/// Resets the service, clearing the cache and re-connecting the database.

	virtual void invalidateCaches(int what = INVALIDATE_ALL) = 0;
		/// Invalidates cached data. The parameter specifies
		/// which caches to invalidate.

	// Service
	const std::type_info& type() const;
	bool isA(const std::type_info& otherType) const;

	static const std::string SERVICE_NAME;
	static const std::string USER_ATTR_FULL_NAME;
	static const std::string USER_ATTR_LDAP;
	static const std::string USER_ATTR_SUCCESS_TIME;
	static const std::string ROLE_ATTR_LDAP;

};


} } } // namespace Poco::OSP::Auth


#endif // OSP_Auth_AuthAdminService_INCLUDED
