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
#include "Poco/Timespan.h"

#include <memory>


namespace Poco::OSP::Auth {


class LDAPClientImpl;


class OSPAuth_API LDAPClient
	/// A client for authenticating users against an LDAP server and
	/// reading their roles, permissions and attributes.
	///
	/// The operations that contact the server are virtual, so that a
	/// subclass can replace them, for example in tests.
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

	static constexpr std::size_t MAX_EXTRA_ATTRIBUTES = 8;
		/// The maximum number of extra attributes (see setExtraAttributesMap())
		/// that are read from the LDAP server.

	explicit LDAPClient(Poco::Logger& logger);
		/// Creates the LDAPClient that implements communication with LDAP server.

	virtual ~LDAPClient();

	LDAPClient() = delete;
	LDAPClient(const LDAPClient&) = delete;
	LDAPClient(LDAPClient&&) = delete;
	LDAPClient& operator = (const LDAPClient&) = delete;
	LDAPClient& operator = (LDAPClient&&) = delete;

	[[nodiscard]] static std::string escapeLDAP(const std::string& str);
		/// Escapes the given string for use as an attribute value
		/// in a distinguished name according to RFC 4514, including
		/// a leading or trailing space and the NUL character.

	[[nodiscard]] static std::string escapeFilterValue(const std::string& str);
		/// Escapes the given string for use as an LDAP search filter
		/// value according to RFC 4515.

	[[nodiscard]] static bool isValidBindValue(const std::string& value);
		/// Returns true if the given user name or password can be sent to
		/// the LDAP server: it is not empty and contains no NUL character.
		/// The LDAP library sends the value as a C string, so a NUL character
		/// would shorten it, down to an empty password for an
		/// unauthenticated bind (RFC 4513, section 5.1.2).

	[[nodiscard]] static Result bindResult(int resultCode);
		/// Returns the Result of a bind that ended with the given result
		/// code of the LDAP library:
		///
		///   - SUCCESS_RESULT for success.
		///   - WRONG_CREDENTIALS if the server rejects the credentials or the
		///     account: invalidCredentials (49, also for a locked, disabled
		///     or expired account in Active Directory), inappropriateAuthentication
		///     (48), insufficientAccessRights (50), unwillingToPerform (53) and
		///     constraintViolation (19, for example from a password policy).
		///   - SERVER_INACCESSIBLE if the server cannot answer now: busy (51),
		///     unavailable (52), timeLimitExceeded (3), and a connection that
		///     fails, breaks or times out before the result arrives.
		///   - ERROR_RESULT for any other result.

	[[nodiscard]] virtual bool initialize(const std::string& uri);
		/// Initializes the connection to the LDAP server

	[[nodiscard]] virtual bool setTimeLimit(int limitSec);
		/// Sets LDAP time limit option that defines the time limit after
		/// which a search operation should be terminated by the server.

	[[nodiscard]] virtual bool setTimeout(const Poco::Timespan& timeout);
		/// Sets the client-side timeout for establishing the connection
		/// to the server and for waiting for the result of each operation.
		/// A bind that times out fails with Result::SERVER_INACCESSIBLE.
		///
		/// Must be called after initialize().

	[[nodiscard]] virtual Result bind(const std::string& username, const std::string& credentials);
		/// Performs LDAP bind for user and credentials using the defined mode.
		///
		/// In DIST_NAME mode, the bind DN is the bind DN template (see setBindDN())
		/// with the escaped username in place of the "%s" placeholder. If no bind
		/// DN template is set, the search base is used as the template.
		///
		/// A username or credentials that are not valid bind values (see
		/// isValidBindValue()) are rejected with Result::WRONG_CREDENTIALS
		/// without contacting the server.

	[[nodiscard]] virtual Result bindDirect(const std::string& bindName, const std::string& credentials);
		/// Performs LDAP bind using the given bind name as-is.
		///
		/// Credentials that are not a valid bind value (see isValidBindValue())
		/// are rejected with Result::WRONG_CREDENTIALS without contacting the server.

	[[nodiscard]] bool isBound() const;
		/// Did the client successfully bound already?

	virtual void unbind();
		/// Unbinds and disconnects from the server

	void setMode(Mode mode);
	[[nodiscard]] Mode mode() const;

	void setBindDN(const std::string& bindDN);
		/// Sets the template of the user DN for DIST_NAME mode, with a "%s"
		/// placeholder for the username.

	[[nodiscard]] const std::string& bindDN() const;

	void setSearchBase(const std::string& base);
	[[nodiscard]] const std::string& searchBase() const;

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
		/// Mapping other LDAP attribute name to Auth attribute names.
		///
		/// Only the first MAX_EXTRA_ATTRIBUTES attributes are read from the server.

	[[nodiscard]] virtual bool findUserDN(const std::string& username, std::string& userDN);
		/// Resolves the distinguished name for the given account name.
		///
		/// Returns false if no entry matches, and without contacting the
		/// server if the username is not a valid bind value (see
		/// isValidBindValue()). Throws a Poco::IOException if the search
		/// fails or finds more than one entry.
		///
		/// In ACCOUNT_NAME mode, also reads the names of the entry (see
		/// accountName(), userPrincipalName() and principalName()).

	[[nodiscard]] virtual bool fetchOwnProperties();
		/// Get roles, permissions, attributes for the authenticated user
		///
		/// In ACCOUNT_NAME mode, also reads the names of the entry (see
		/// accountName(), userPrincipalName() and principalName()), and
		/// throws a Poco::IOException if the entry has more than one
		/// sAMAccountName value.

	[[nodiscard]] virtual bool fetchOwnProperties(const std::string& userDN);
		/// Get roles, permissions, attributes for the authenticated user
		/// by querying the given distinguished name directly.
		///
		/// In ACCOUNT_NAME mode, also reads the names of the entry (see
		/// accountName(), userPrincipalName() and principalName()), and
		/// throws a Poco::IOException if the entry has more than one
		/// sAMAccountName value.

	[[nodiscard]] virtual bool fetchGroupPermissions(const std::string& groupDN, Permissions& permissions);
		/// Get permissions assigned to an LDAP group using its DN.
		/// It only works if the role attribute name resolves to an DN, for example
		/// when using memberOf. It does not read and role attributes otherwise
		/// because the LDAP entry is not found.

	[[nodiscard]] const std::string& fullName() const;
		/// Full user name (if it is available) obtained with last query

	[[nodiscard]] const std::string& accountName() const;
		/// The sAMAccountName of the user's entry read by the last call of
		/// findUserDN() or fetchOwnProperties() in ACCOUNT_NAME mode. Empty if
		/// the entry has none, in DIST_NAME mode, or if the entry was not read.
		///
		/// An entry with more than one sAMAccountName value is a directory
		/// error: findUserDN() and fetchOwnProperties() throw a
		/// Poco::IOException then.

	[[nodiscard]] const std::string& userPrincipalName() const;
		/// The userPrincipalName of the entry read with accountName(). Empty
		/// if the entry has none or more than one, in DIST_NAME mode, or if
		/// the entry was not read.

	[[nodiscard]] const std::string& principalName() const;
		/// The msDS-PrincipalName of the entry read with accountName(): the
		/// name in the form "DOMAIN\sAMAccountName" that Active Directory
		/// constructs. Empty if the entry has none or more than one, in
		/// DIST_NAME mode, or if the entry was not read.

	[[nodiscard]] const Roles& roles() const;
		/// Roles obtained with last query

	[[nodiscard]] const Permissions& permissions() const;
		/// Permissions obtained with last query

	[[nodiscard]] const Attributes& attributes() const;
		/// Attributes obtained with last query

	[[nodiscard]] int lastReturnCode() const;
		/// Return code of the last LDAP operation.

protected:
	void setOwnProperties(std::string fullName, Roles roles, Permissions permissions, Attributes attributes);
		/// Stores the properties returned by fullName(), roles(), permissions()
		/// and attributes().

	void setAccountName(std::string accountName);
		/// Stores the value returned by accountName().

	void setUserPrincipalName(std::string userPrincipalName);
		/// Stores the value returned by userPrincipalName().

	void setPrincipalName(std::string principalName);
		/// Stores the value returned by principalName().

private:
	[[nodiscard]] std::string distinguishedName(const std::string& username) const;
		/// Returns the DN of the given user in DIST_NAME mode.

	std::unique_ptr<LDAPClientImpl> _impl;

	Poco::Logger& _logger;

	Mode _mode { Mode::DIST_NAME };

	std::string _bindDN;
	std::string _username;
	std::string _searchBase;
	std::string _userObjectClass;
	std::string _groupObjectClass;

	std::string _fullName;
	std::string _accountName;
	std::string _userPrincipalName;
	std::string _principalName;
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


inline const std::string& LDAPClient::accountName() const
{
	return _accountName;
}


inline const std::string& LDAPClient::userPrincipalName() const
{
	return _userPrincipalName;
}


inline const std::string& LDAPClient::principalName() const
{
	return _principalName;
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


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_LDAPClient_INCLUDED
