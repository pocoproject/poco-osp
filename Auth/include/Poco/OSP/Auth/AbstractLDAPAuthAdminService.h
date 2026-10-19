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
#include "Poco/Logger.h"


namespace Poco {
namespace OSP {
namespace Auth {


class OSPAuth_API AbstractLDAPAuthAdminService: public Poco::OSP::Auth::AuthAdminService
	/// This abstract class implements basic LDAP authentication.
{
public:
	struct LDAPParams
		/// LDAP authentication parameters.
	{
		std::string uri;
			/// URI of the LDAP server, e.g. "ldap://localhost".

		std::string bindDN;
			/// The bind DN for binding to the LDAP server, with
			/// the actual username replaced by a "%s" placeholder,
			/// e.g. "uid=%s,cn=users,dc=ldapserver,dc=appinf,dc=com".

		std::string bindUser;
			/// Optional service account used in ACCOUNT mode to search
			/// for the user's distinguished name before rebinding as
			/// the user.

		std::string bindPassword;
			/// Password for bindUser.

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

		int timeout;
			/// The timeout in seconds for binding to the LDAP server.

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
			/// be queried via LDAP.

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

	~AbstractLDAPAuthAdminService();
		/// Destroys the AuthAdminServiceImpl.

	static std::string escapeLDAP(const std::string& str);
		/// Escapes the given string according to LDAP rules.

protected:
	bool authenticateLDAP(const std::string& username, const std::string& credentials, bool& fallbackLocal) const;
	bool isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const;
	virtual void createOrUpdateRoleImpl(const std::string& rolename) const = 0;
	virtual void createOrUpdateUserImpl(const std::string& username, const std::string& credentials) const = 0;
	virtual void replaceUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& value) const = 0;
	virtual void replaceRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& value) const = 0;
	virtual void replacePermissionsForUserImpl(const std::string& username, const std::set<std::string>& permissions) const = 0;
	virtual void replaceRolesForUserImpl(const std::string& username, const std::set<std::string>& permissions) const = 0;
	virtual void replacePermissionsForRoleImpl(const std::string& rolename, const std::set<std::string>& permissions) const = 0;
	virtual std::string getUserAttributeImpl(const std::string& username, const std::string& attribute, const std::string& deflt = ""s) const = 0;
	virtual std::string getRoleAttributeImpl(const std::string& rolename, const std::string& attribute, const std::string& deflt = ""s) const = 0;


protected:
	Poco::Logger& _logger;
	LDAPParams _ldapParams;
};


} } } // namespace Poco::OSP::Auth


#endif // OSP_Auth_AbstractLDAPAuthAdminService_INCLUDED
