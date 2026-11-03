//
// AbstractLDAPAuthAdminService.h
//
// Library: OSP
// Package: Auth
// Module:  AbstractLDAPAuthAdminService
//
// Definition of the AbstractLDAPAuthAdminService class.
//
// Copyright (c) 2012-2017, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_AbstractLDAPAuthAdminService_INCLUDED
#define OSP_Auth_AbstractLDAPAuthAdminService_INCLUDED


#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/Auth/AuthTypes.h"
#include "Poco/Logger.h"
#include "Poco/Types.h"
#include <map>
#include <memory>
#include <optional>
#include <string>


namespace Poco::OSP::Auth {


class LDAPClient;


class OSPAuth_API AbstractLDAPAuthAdminService: public Poco::OSP::Auth::AuthAdminService
	/// This abstract class implements basic LDAP authentication.
{
public:
	struct LDAPParams
		/// LDAP authentication parameters.
	{
		std::string uri;
			/// URI of the LDAP server, e.g. "ldaps://ldap.example.com".
			/// Only an "ldaps://" URI keeps the passwords off the network
			/// in clear text; StartTLS is not supported.

		std::string bindDN;
			/// The bind DN for binding to the LDAP server, with
			/// the actual username replaced by a "%s" placeholder,
			/// e.g. "uid=%s,cn=users,dc=ldapserver,dc=appinf,dc=com".

		std::string bindUser;
			/// Optional service account used in ACCOUNT mode to search
			/// for the user's distinguished name before rebinding as
			/// the user.

		std::string bindPassword;
			/// Password for bindUser. Must not be empty if bindUser is used.

		std::string bindMode;
			/// In ACCOUNT mode, controls which account performs LDAP
			/// searches and reads:
			/// USER: end user performs search and reads
			/// SERVICE: bindUser performs search and reads while the
			///          end user is only rebound for credential
			///          verification.

		std::string searchBase;
			/// The search base for querying LDAP attributes. Usually
			/// equal to bindDN.

		std::string userObjectClass;
			/// User objectClass:
			/// Windows/AD: user
			/// OpenLDAP: inetOrgPerson

		std::string groupObjectClass;
			/// Group objectClass:
			/// Windows/AD: group
			/// OpenLDAP: groupOfNames

		std::string authMode;
			/// Authentication mode:
			/// DN: use distiguished name and bindDN
			/// ACCOUNT: Use userPrincipalName (like "John.Minor@the-company.com") or sAMAccountName (like "COMPANY\John.Minor")

		static constexpr int DEFAULT_TIMEOUT = 30;

		int timeout = DEFAULT_TIMEOUT;
			/// The timeout in seconds for connecting to the LDAP server and
			/// for each LDAP operation. The server enforces it as the time
			/// limit of a search, the client as the time it waits for the
			/// connection and for each result. An LDAP server that does not
			/// answer within the timeout is treated as inaccessible.
			/// A value of 0 or less is replaced by DEFAULT_TIMEOUT, and a
			/// warning is logged.

		std::string permissionAttribute;
			/// The LDAP attribute used for storing permission values.
			/// Can be empty if permissions are not provided via LDAP.

		std::string roleAttribute;
			/// The LDAP attribute used for storing role assignments.
			/// Can be empty if roles are not provided via LDAP.
			///
			/// Only used if also a permissionAttribute is given.

		std::string rolePermissionAttribute;
			/// The LDAP attribute used for storing role permission values.
			/// Can be empty if permissions are not provided via LDAP.

		std::vector<std::pair<std::string, std::string>> extraAttributes;
			/// The name pairs (mapping LDAP attribute to user attribute name)
			/// of a extra LDAP attributes to query from the LDAP server.
			///
			/// If non-empty, the values of these attributes are obtained
			/// when authenticating the user and can be obtained
			/// by calling getUserAttribute().
			///
			/// At most 8 attributes may be specified.

		std::string localUsers;
			/// A regular expression for matching usernames that should not
			/// be queried via LDAP.

		std::string localRoles;
			/// A regular expression for matching rolenames that should not
			/// be queried via LDAP. An LDAP role matching it is neither
			/// imported nor assigned to the user, so a local role with the
			/// same name keeps its permissions. The same holds for a role
			/// whose ROLE_ATTR_LDAP attribute is "false", which marks a role
			/// created locally while LDAP authentication is enabled.

		std::string roleFilter;
			/// A regular expression for matching rolenames that should be imported
			/// and assigned from LDAP. If empty, all roles are imported.

		bool localFallback = false;
			/// When server is not accessible, try authenticating with locally stored
			/// credentials if they are not too old.

		int uNameLifeDaysDays = 7;
			/// Used in conjunction with localFallback. Users's cached credentials
			/// are verified only if they were not saved to database too long ago.
	};

	AbstractLDAPAuthAdminService(Poco::Logger& logger, const LDAPParams& ldapParams);
		/// Creates the AbstractLDAPAuthAdminService using the given LDAP parameters.
		///
		/// Note: if ldapParams.uri is empty, LDAP authentication will be disabled.

	~AbstractLDAPAuthAdminService() override;
		/// Destroys the AuthAdminServiceImpl.

protected:
	enum class LDAPResult: Poco::UInt8
		/// The answer of the LDAP server to the credentials of a user.
	{
		ACCEPTED,
		REJECTED,
		INACCESSIBLE,
		REFUSED,
			/// The server answers the bind of the user with a result that
			/// neither accepts nor rejects the credentials, or, in the USER
			/// bind mode, accepts the bind of a name that the entry found
			/// for that name does not have (see verifyLDAP()).
		LOCKED_OUT
			/// acceptLDAPAccount() refuses the account of the user: in the
			/// SERVICE bind mode before the bind of the user, which is then
			/// not made, in the USER bind mode after the bind has accepted
			/// the password.
	};

	struct LDAPUser
		/// The properties of a user read from the LDAP server.
	{
		std::string fullName;
		Permissions permissions;
		Attributes attributes;
		bool hasRoles = false;
			/// True if the LDAP server lists roles for the user, also
			/// if none of them is imported.
		std::map<std::string, std::optional<Permissions>> roles;
			/// The roles to import, with the permissions of each role, or
			/// without a value if they cannot be read.
		bool passwordVerified = false;
			/// True once the LDAP server has accepted the password, also if
			/// reading the user's properties fails afterwards.
		std::string accountName;
			/// The account name of the user's entry in the ACCOUNT mode (see
			/// LDAPClient::accountName()), read by the search of the SERVICE
			/// bind mode and again with the user's properties. Empty if the
			/// entry has none or it is not read.
	};

	[[nodiscard]] bool authenticateLDAP(const std::string& username, const std::string& credentials,
		bool& fallbackLocal) const;
		/// Authenticates the user with the LDAP server and stores the user's
		/// password, roles, permissions and attributes obtained from the server,
		/// with verifyLDAP(), storeLDAPUser() and canUseLocalCredentials().
		/// The user is stored under the account name in lower case if
		/// verifyLDAP() has read it (see LDAPUser::accountName), otherwise
		/// under username as given.
		///
		/// Returns false if the server rejects or refuses the credentials,
		/// finds no entry for the user, or cannot be reached, or if
		/// acceptLDAPAccount() refuses the account. Only if it cannot
		/// be reached, fallbackLocal is set to true where the locally stored
		/// credentials may be used instead (see canUseLocalCredentials()).
		///
		/// Throws a Poco::IOException if the LDAP directory cannot be used,
		/// as verifyLDAP() describes, and, before anything is stored, if the
		/// permissions of one of the user's groups cannot be read (see
		/// storeLDAPUser()). An exception of the functions that store the
		/// user, such as a Poco::Data::DataException, is passed on and can
		/// leave the user partly stored, as storeLDAPUser() describes.

	[[nodiscard]] LDAPResult verifyLDAP(const std::string& username, const std::string& credentials,
		LDAPUser& user) const;
		/// Verifies the credentials with the LDAP server and, if the server
		/// accepts them, reads the user's properties into user. Does not use
		/// the local store, so a subclass can call it without holding its lock.
		///
		/// A user name or password that is empty or contains a NUL character
		/// is rejected without contacting the server.
		///
		/// Throws a Poco::IOException if the LDAP server cannot be used to
		/// verify the credentials: the service account of the SERVICE bind
		/// mode has an empty password or is rejected, the search for the
		/// user's entry fails or finds more than one entry, the timeouts of
		/// the connection cannot be set, or the user's entry cannot be read
		/// after the server has accepted the credentials. Finding no entry
		/// for the user is a rejection. user.passwordVerified tells whether
		/// the server had accepted the password before the exception.
		///
		/// The answer of the server to the bind of the user is mapped with
		/// LDAPClient::bindResult(): a rejection of the credentials is
		/// LDAPResult::REJECTED, a server that cannot answer now is
		/// LDAPResult::INACCESSIBLE, and any other result is LDAPResult::REFUSED.
		///
		/// In the ACCOUNT mode, user.accountName holds the account name of the
		/// user's entry. verifyLDAP() calls acceptLDAPAccount() as soon as the
		/// account name is known, and if it refuses the account, the result is
		/// LDAPResult::LOCKED_OUT. In the SERVICE bind mode, the account name
		/// is read by the search before the bind of the user, which is then
		/// not made. In the USER bind mode, it is read only after the server
		/// has accepted the password, so a rejected password cannot be
		/// attributed to the account of another name of the user; the SERVICE
		/// bind mode is needed where a local lock-out must cover every name of
		/// a user.
		///
		/// The USER bind mode binds with the given name, which the server may
		/// accept for another identity than the entry found by that name, such
		/// as an identity of another domain. So after the bind, the entry is
		/// used only if the given name is one of its names, compared ignoring
		/// case: a name with "\" is the msDS-PrincipalName of the entry
		/// ("DOMAIN\name", see LDAPClient::principalName()), a name with "@"
		/// its userPrincipalName, and any other name its sAMAccountName.
		/// Otherwise the result is LDAPResult::REFUSED, and a warning is logged.
		/// A "DOMAIN\name" login therefore needs a directory that provides
		/// msDS-PrincipalName. The server binds a name without "\" and "@" in
		/// its own domain; if the search base spans several domains that have
		/// an entry with that sAMAccountName, the search finds several
		/// entries, and verifyLDAP() throws a Poco::IOException, as the LDAP
		/// directory cannot be used. The SERVICE bind mode binds as the DN of
		/// the entry found, and the DN mode as the DN built from the name, so
		/// they need no such check.

	[[nodiscard]] virtual bool acceptLDAPAccount(const std::string& userName, const std::string& accountName) const;
		/// Returns whether verifyLDAP() may continue the login of userName, the
		/// name given to verifyLDAP(), whose LDAP entry has the given account
		/// name, in lower case. If the account name is not determined (the DN
		/// mode, or an entry without one), accountName is userName in lower
		/// case. verifyLDAP() calls it in the SERVICE bind mode after the search
		/// has found the user's entry and before the bind of the user, so that
		/// the password of a locked-out account is not tried through another
		/// name of the user, and in the USER bind mode after the bind and the
		/// read of the entry.
		///
		/// It is called without a lock held by verifyLDAP() or its caller, so
		/// an implementation takes its own lock.
		///
		/// The default implementation returns true.

	void storeLDAPUser(const std::string& username, const std::string& credentials, const LDAPUser& user) const;
		/// Stores the password, attributes, permissions and roles of a user
		/// that the LDAP server has accepted. A role whose ROLE_ATTR_LDAP
		/// attribute is "false" is not imported.
		///
		/// Throws a Poco::IOException, before anything is stored, if the
		/// permissions of a role to import could not be read.
		///
		/// An exception of the functions that store the user, such as a
		/// Poco::Data::DataException, is passed on after a part of the user
		/// may have been stored. A subclass that stores the user in a
		/// transaction rolls it back then, as AuthAdminServiceImpl does.

	[[nodiscard]] bool canUseLocalCredentials(const std::string& username) const;
		/// Returns true if the locally stored credentials of the user may be
		/// used while the LDAP server cannot be reached or the LDAP directory
		/// cannot be used (see LDAPParams::localFallback and
		/// LDAPParams::uNameLifeDaysDays).

#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	[[nodiscard]] virtual std::unique_ptr<LDAPClient> createLDAPClient() const;
		/// Creates the LDAPClient that authenticateLDAP() uses for one
		/// connection to the LDAP server.
		///
		/// The default implementation creates an LDAPClient that logs to
		/// the logger of the service.
#endif

	[[nodiscard]] bool isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const;
	virtual void createOrUpdateRoleImpl(const std::string& rolename) const = 0;
	virtual void createOrUpdateUserImpl(const std::string& username, const std::string& credentials) const = 0;
	virtual void replaceUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& value) const = 0;
	virtual void replaceRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& value) const = 0;
	virtual void replacePermissionsForUserImpl(const std::string& username, const std::set<std::string>& permissions) const = 0;
	virtual void replaceRolesForUserImpl(const std::string& username, const std::set<std::string>& permissions) const = 0;
	virtual void replacePermissionsForRoleImpl(const std::string& rolename, const std::set<std::string>& permissions) const = 0;
	[[nodiscard]] virtual std::string getUserAttributeImpl(const std::string& username, const std::string& attribute,
		const std::string& deflt = std::string()) const = 0;
	[[nodiscard]] virtual std::string getRoleAttributeImpl(const std::string& rolename, const std::string& attribute,
		const std::string& deflt = std::string()) const = 0;


protected:
	Poco::Logger& _logger;
	LDAPParams _ldapParams;
};


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_AbstractLDAPAuthAdminService_INCLUDED
