//
// LDAPClient.h
//
// Library: OSP
// Package: Auth
// Module:  LDAPClient
//
// Definition of the LDAPClient class.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_LDAPClient_INCLUDED
#define OSP_Auth_LDAPClient_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "Poco/OSP/Auth/AuthTypes.h"
#include "Poco/Logger.h"

#include <memory>


namespace Poco {
namespace OSP {
namespace Auth {


class LDAPClientImpl;


class OSPAuth_API LDAPClient
{
public:

	// Type aliases for backwards compatibility
	using ValueMap = Poco::OSP::Auth::ValueMap;
	using ValueSet = Poco::OSP::Auth::ValueSet;
	using Roles = Poco::OSP::Auth::Roles;
	using Permissions = Poco::OSP::Auth::Permissions;
	using Attributes = Poco::OSP::Auth::Attributes;
	using AttributesNameMap = Poco::OSP::Auth::AttributesNameMap;

	enum class Mode
		/// What method of authentication shall be used
	{
		DIST_NAME,		/// Use Distinguished Name and bindDN patttern ("John Minor" and "uid=%s,cn=users,dc=ldapserver,dc=appinf,dc=com")
		ACCOUNT_NAME	/// Use userPrincipalName (like "John.Minor@the-company.com") or sAMAccountName (like "COMPANY\John.Minor")
	};

	enum class Result
	{
		SUCCESS_RESULT,
		SERVER_INACCESSIBLE,
		WRONG_CREDENTIALS,
		ERROR_RESULT
	};

	LDAPClient(Poco::Logger& logger);
		/// Creates the LDAPClient that implements communication with LDAP server.

	~LDAPClient();

	static std::string escapeLDAP(const std::string& str);
		/// Escapes the given string according to LDAP rules.

	static std::string escapeFilterValue(const std::string& str);
		/// Escapes the given string for use as an LDAP search filter
		/// value according to RFC 4515.

	bool initialize(const std::string& uri);
		/// Initializes the connection to the LDAP server

	bool setTimeLimit(int limitSec);
		/// Sets LDAP time limit option that defines the time limit after
		/// which a search operation should be terminated by the server.

	Result bind(const std::string& username, const std::string& credentials);
		/// Performs LDAP bind for user and credentials using the defined mode

	Result bindDirect(const std::string& bindName, const std::string& credentials);
		/// Performs LDAP bind using the given bind name as-is.

	bool isBound() const;
		/// Did the client successfully bound already?

	void unbind();
		/// Unbinds and disconnects from the server

	void setMode(Mode mode);
	Mode mode() const;

	void setBindDN(const std::string& bindDN);
	const std::string& bindDN() const;

	void setSearchBase(const std::string& base);
	const std::string& searchBase() const;

	void setUserObjectClass(const std::string& userObjectClass);
		/// LDAP attribute user object class

	void setGroupObjectClass(const std::string& groupObjectClass);
		/// LDAP attribute group object class

	void setRoleAttributeName(const std::string& name);
		/// LDAP attribute name used for roles

	void setPermissionAttributeName(const std::string& name);
		/// LDAP attribute name used for permissions

	void setRolePermissionAttributeName(const std::string& name);
		/// LDAP attribute name used for role permissions

	void setExtraAttributesMap(const AttributesNameMap& nameMap);
		/// Mapping other LDAP attribute name to Auth attribute names

	bool findUserDN(const std::string& username, std::string& userDN);
		/// Resolves the distinguished name for the given account name.

	bool fetchOwnProperties();
		/// Get roles, permissions, attributes for the authenticated user

	bool fetchOwnProperties(const std::string& userDN);
		/// Get roles, permissions, attributes for the authenticated user
		/// by querying the given distinguished name directly.

	bool fetchGroupPermissions(const std::string& groupDN, Permissions& permissions);
		/// Get permissions assigned to an LDAP group using its DN.
		/// It only works if the role attribute name resolves to an DN, for example
		/// when using memberOf. It does not read and role attributes otherwise
		/// because the LDAP entry is not found.

	const std::string& fullName() const;
		/// Full user name (if it is available) obtained with last query

	const Roles& roles() const;
		/// Roles obtained with last query

	const Permissions& permissions() const;
		/// Permissions obtained with last query

	const Attributes& attributes() const;
		/// Attributes obtained with last query

	int lastReturnCode() const;
		/// Return code of the last LDAP operation.

private:

	std::unique_ptr<LDAPClientImpl> _impl;

	Poco::Logger& _logger;

	Mode _mode { Mode::DIST_NAME };

	std::string _bindDN;
	std::string _username;
	std::string _searchBase;
	std::string _userObjectClass;
	std::string _groupObjectClass;

	std::string _fullName;
	Roles _roles;
	Permissions _permissions;
	Attributes _attributes;
};


inline void LDAPClient::setBindDN(const std::string& bindDN)
{
	_bindDN = bindDN;
}


inline const std::string& LDAPClient::bindDN() const
{
	return _bindDN;
}


inline void LDAPClient::setSearchBase(const std::string& base)
{
	_searchBase = base;
}


inline const std::string& LDAPClient::searchBase() const
{
	return _searchBase;
}


inline void LDAPClient::setUserObjectClass(const std::string& userObjectClass)
{
	_userObjectClass = userObjectClass;
}


inline void LDAPClient::setGroupObjectClass(const std::string& groupObjectClass)
{
	_groupObjectClass = groupObjectClass;
}


inline const std::string& LDAPClient::fullName() const
{
	return _fullName;
}


inline const LDAPClient::Roles& LDAPClient::roles() const
{
	return _roles;
}


inline const LDAPClient::Permissions& LDAPClient::permissions() const
{
	return _permissions;
}


inline const LDAPClient::Attributes& LDAPClient::attributes() const
{
	return _attributes;
}


} } } // namespace Poco::OSP::Auth


#endif // OSP_Auth_LDAPClient_INCLUDED
