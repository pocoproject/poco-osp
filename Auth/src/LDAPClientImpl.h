//
// LDAPClientImpl.h
//
// Library: OSP
// Package: Auth
// Module:  LDAPClient
//
// Definition of the LDAPClientImpl class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_LDAPClientImpl_INCLUDED
#define OSP_Auth_LDAPClientImpl_INCLUDED


#include "Poco/OSP/Auth/LDAPClient.h"
#include "Poco/Logger.h"
#include "Poco/Timespan.h"
#if defined(POCO_OS_FAMILY_WINDOWS)
#include <windows.h>
#include <winldap.h>
#else
#include <ldap.h>
#endif
#include <string>
#include <vector>


namespace Poco::OSP::Auth {


class LDAPClientImpl
	/// The connection of an LDAPClient to the LDAP server, with OpenLDAP
	/// (LDAPClient_POSIX.cpp) or WinLDAP (LDAPClient_WIN32.cpp). The members
	/// that do not use the LDAP library are defined in LDAPClient.cpp.
{
public:
	using Roles = LDAPClient::Roles;
	using Permissions = LDAPClient::Permissions;
	using Attributes = LDAPClient::Attributes;
	using AttributesNameMap = LDAPClient::AttributesNameMap;
	using Mode = LDAPClient::Mode;
	using Result = LDAPClient::Result;

	struct AuthProperties
		/// The properties of the entry found by find().
	{
		std::string distinguishedName;
		std::string fullName;
		Roles roles;
		Permissions permissions;
		Attributes attributes;
		std::vector<std::string> accountNames;
			/// The values of sAMAccountName, if it is queried.
		std::vector<std::string> userPrincipalNames;
			/// The values of userPrincipalName, queried with sAMAccountName.
		std::vector<std::string> principalNames;
			/// The values of msDS-PrincipalName, queried with sAMAccountName.
	};

	explicit LDAPClientImpl(Poco::Logger& logger);
		/// Creates the LDAPClientImpl without a connection.

	~LDAPClientImpl();
		/// Unbinds and closes the connection, if there is one.

	LDAPClientImpl() = delete;
	LDAPClientImpl(const LDAPClientImpl&) = delete;
	LDAPClientImpl(LDAPClientImpl&&) = delete;
	LDAPClientImpl& operator = (const LDAPClientImpl&) = delete;
	LDAPClientImpl& operator = (LDAPClientImpl&&) = delete;

	[[nodiscard]] bool initialize(const std::string& uri);
		/// Prepares the connection to the LDAP server with the given URI.

	[[nodiscard]] bool setTimeLimit(int limit);
		/// Sets the time limit of a search in seconds, which the server enforces.

	[[nodiscard]] Result bind(const std::string& bindDN, const std::string& credentials);
		/// Binds with the given DN or name and password.

	[[nodiscard]] bool isBound() const;
		/// Returns true if the last bind has succeeded.

	void unbind();
		/// Unbinds and closes the connection.

	[[nodiscard]] bool setTimeout(const Poco::Timespan& timeout);
		/// Sets the timeout for connecting and for the result of each operation;
		/// WinLDAP also uses it for sending each request.

	void setRoleAttributeName(const std::string& name);
		/// Sets the attribute that holds the roles of a user.

	void setPermissionAttributeName(const std::string& name);
		/// Sets the attribute that holds the permissions of a user.

	void setRolePermissionAttributeName(const std::string& name);
		/// Sets the attribute that holds the permissions of a group.

	void setExtraAttributesMap(const AttributesNameMap& nameMap);
		/// Sets the other attributes to read, with their names in the properties.

	[[nodiscard]] bool find(const std::string& searchBase, const std::string& filter, bool subtree,
		bool rolePermissions, bool entryRequired, bool queryAccountName, AuthProperties& properties);
		/// Searches the entry that matches the filter, at searchBase or, if
		/// subtree is true, in the subtree below it, and stores its properties.
		/// The search requests the attribute of the permissions of a group if
		/// rolePermissions is true, otherwise the one of the permissions of a
		/// user, and the names of the ACCOUNT mode if queryAccountName is true.
		///
		/// Returns false if the search fails or finds more than one entry, or
		/// no entry while entryRequired is true. Finding no entry otherwise
		/// stores empty properties.

	[[nodiscard]] int lastReturnCode() const;
		/// Return code of the last LDAP operation.

private:
	void addValue(const std::string& attr, std::string value, bool queryAccountName,
		AuthProperties& properties) const;
		/// Adds a value of the given attribute of the entry found by find() to
		/// properties, without leading and trailing white space.

	[[nodiscard]] bool checkEntryCount(int entryCount, bool entryRequired, const std::string& searchBase,
		const std::string& filter) const;
		/// Returns false, and logs why, if a search has found more than one
		/// entry, or none while entryRequired is true.

	[[nodiscard]] bool isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const;

	static const std::string DISPLAY_NAME_ATTR;
	static const std::string ACCOUNT_NAME_ATTR;
	static const std::string USER_PRINCIPAL_NAME_ATTR;
	static const std::string PRINCIPAL_NAME_ATTR;
	static constexpr long DEFAULT_TIMEOUT_SECONDS = 30;

	Poco::Logger& _logger;
	::LDAP* _ldap{nullptr};
	bool _bindSuccessful{false};
#if defined(POCO_OS_FAMILY_WINDOWS)
	ULONG _rc{0};
#else
	int _rc{0};
#endif

	Poco::Timespan _timeout{DEFAULT_TIMEOUT_SECONDS, 0};
#if defined(POCO_OS_FAMILY_WINDOWS)
	Poco::Timespan _bindTimeout;
		/// The limit for connecting and for the result of a bind; zero
		/// until setTimeout() is called, and the bind then waits without a limit.
#endif

	std::string _roleAttributeName;
	std::string _permissionAttributeName;
	std::string _rolePermissionAttributeName;
	AttributesNameMap _extraAttributesMap;
};


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_LDAPClientImpl_INCLUDED
