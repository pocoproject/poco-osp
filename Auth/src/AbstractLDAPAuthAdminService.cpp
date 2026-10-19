//
// AbstractLDAPAuthAdminService.cpp
//
// Library: OSP
// Package: Auth
// Module:  AbstractLDAPAuthAdminService
//
// Copyright (c) 2012-2017, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/AbstractLDAPAuthAdminService.h"
#include "Poco/UTF8String.h"
#include "Poco/String.h"
#include "Poco/NumberParser.h"
#include "Poco/RegularExpression.h"

#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	#include "Poco/OSP/Auth/LDAPClient.h"
#endif

#include <algorithm>

using namespace std::string_literals;


namespace Poco {
namespace OSP {
namespace Auth {


AbstractLDAPAuthAdminService::AbstractLDAPAuthAdminService(Poco::Logger& logger, const LDAPParams& ldapParams):
	_logger(logger),
	_ldapParams(ldapParams)
{
}


AbstractLDAPAuthAdminService::~AbstractLDAPAuthAdminService()
{
}


bool AbstractLDAPAuthAdminService::authenticateLDAP(const std::string& username, const std::string& credentials, bool& fallbackLocal) const
{
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)

	fallbackLocal = false;

	LDAPClient lc(_logger);

	auto configureClient = [this](LDAPClient& client)
	{
		if (_ldapParams.authMode == "DN"s)
			client.setMode(LDAPClient::Mode::DIST_NAME);
		else if (_ldapParams.authMode == "ACCOUNT"s)
			client.setMode(LDAPClient::Mode::ACCOUNT_NAME);
		else
			_logger.information("LDAP authentication mode not known: %s", _ldapParams.authMode);

		client.setBindDN(_ldapParams.bindDN);
		client.setSearchBase(_ldapParams.searchBase);
		client.setUserObjectClass(_ldapParams.userObjectClass);
		client.setGroupObjectClass(_ldapParams.groupObjectClass);
		client.setPermissionAttributeName(_ldapParams.permissionAttribute);
		client.setRolePermissionAttributeName(_ldapParams.rolePermissionAttribute);
		client.setRoleAttributeName(_ldapParams.roleAttribute);
		client.setExtraAttributesMap(_ldapParams.extraAttributes);
	};
	configureClient(lc);

	auto canDoFallback = [this, &username]() -> bool
	{
		if (!_ldapParams.localFallback)
		{
			return false;
		}
		/// Try db-cached authentication if LDAP server if conditions are right
		const auto lastTimeStr = getUserAttributeImpl(username, USER_ATTR_SUCCESS_TIME);
		if (lastTimeStr.empty())
		{
			return false;
		}
		Poco::Int64 lastTime;
		if (!Poco::NumberParser::tryParse64(lastTimeStr, lastTime))
		{
			return false;
		}
		const Poco::Int64 uNameLifeSecs {_ldapParams.uNameLifeDaysDays * 60*60*24};
		Poco::Timestamp now;
		if ( (now.epochTime() - lastTime) < uNameLifeSecs)
		{
			_logger.information("LDAP not accessible. Fallback to saved credentials: %s", username);
			return true;
		}
		return false;
	};

	auto initializeConnection = [this, &canDoFallback, &fallbackLocal](LDAPClient& client) -> bool
	{
		if (!client.initialize(_ldapParams.uri))
		{
			fallbackLocal = canDoFallback();
			return false;
		}
		if (!client.setTimeLimit(_ldapParams.timeout))
		{
			return false;
		}
		return true;
	};

	std::string userDN;
	std::string creds(credentials);
	LDAPClient::Result result = LDAPClient::Result::ERROR_RESULT;
	bool useServiceBind = false;

	if (lc.mode() == LDAPClient::Mode::ACCOUNT_NAME)
	{
		const auto bindMode = Poco::toUpper(_ldapParams.bindMode);
		if (!_ldapParams.bindUser.empty())
		{
			if (bindMode.empty() || bindMode == "SERVICE"s)
			{
				useServiceBind = true;
			}
			else if (bindMode != "USER"s)
			{
				_logger.warning("LDAP bind mode not known: %s. Using SERVICE because auth.ldap.bindUser is configured.", _ldapParams.bindMode);
				useServiceBind = true;
			}
		}
		else if (bindMode == "SERVICE"s)
		{
			_logger.warning("LDAP bind mode SERVICE requires auth.ldap.bindUser. Falling back to USER mode.");
		}
		else if (!bindMode.empty() && bindMode != "USER"s)
		{
			_logger.warning("LDAP bind mode not known: %s. Using USER mode.", _ldapParams.bindMode);
		}

		if (_logger.information())
		{
			if (useServiceBind)
			{
				_logger.information("LDAP ACCOUNT bind workflow: SERVICE (searches and reads use auth.ldap.bindUser).");
			}
			else
			{
				_logger.information("LDAP ACCOUNT bind workflow: USER (searches and reads use the authenticated user).");
			}
		}
	}

	if (!initializeConnection(lc))
	{
		return false;
	}

	if (useServiceBind)
	{
		std::string bindCreds(_ldapParams.bindPassword);
		const auto bindResult = lc.bindDirect(_ldapParams.bindUser, bindCreds);
		Poco::secureClear(bindCreds);

		if (bindResult != LDAPClient::Result::SUCCESS_RESULT)
		{
			Poco::secureClear(creds);
			if (bindResult == LDAPClient::Result::SERVER_INACCESSIBLE)
			{
				fallbackLocal = canDoFallback();
			}
			return false;
		}

		if (!lc.findUserDN(username, userDN))
		{
			Poco::secureClear(creds);
			return false;
		}

		LDAPClient verifyLC(_logger);
		configureClient(verifyLC);
		if (!initializeConnection(verifyLC))
		{
			Poco::secureClear(creds);
			return false;
		}

		result = verifyLC.bindDirect(userDN, creds);
	}
	else
	{
		result = lc.bind(username, creds);
	}

	Poco::secureClear(creds);

	if (result != LDAPClient::Result::SUCCESS_RESULT)
	{
		if (result == LDAPClient::Result::SERVER_INACCESSIBLE)
		{
			fallbackLocal = canDoFallback();
		}
		return false;
	}

	if (useServiceBind && _logger.debug())
	{
		_logger.debug("Fetching LDAP user properties using the service-account connection.");
	}
	if (!lc.fetchOwnProperties(userDN))
	{
		return false;
	}

	_logger.information("Display name: %s", lc.fullName());
	for (const auto& p: lc.permissions())
	{
		_logger.information("Permission: %s", p);
	}
	for (const auto& r: lc.roles())
	{
		_logger.information("Role: %s", r);
	}
	for (const auto& a: lc.attributes())
	{
		_logger.information("Attribute: %s = %s", a.first, a.second);
	}

	createOrUpdateUserImpl(username, credentials);
	replaceUserAttributeImpl(username, USER_ATTR_FULL_NAME, lc.fullName());

	if (!lc.permissions().empty())
	{
		replacePermissionsForUserImpl(username, lc.permissions());
	}

	for (const auto& a: lc.attributes())
	{
		replaceUserAttributeImpl(username, a.first, a.second);
	}

	if (!lc.roles().empty())
	{
		LDAPClient::Roles roles;
		if (!_ldapParams.roleFilter.empty())
		{
			Poco::RegularExpression roleFilter(_ldapParams.roleFilter);
			for (const auto& r: lc.roles())
			{
				if (roleFilter.match(r))
				{
					roles.insert(r);
				}
				else if (_logger.debug())
				{
					_logger.debug("Skipping LDAP role due to roleFilter: %s", r);
				}
			}
		}
		else
		{
			roles = lc.roles();
		}

		for (const auto& r: roles)
		{
			createOrUpdateRoleImpl(r);
			replaceRoleAttributeImpl(r, ROLE_ATTR_LDAP, "true");
			LDAPClient::Permissions permissions;
			if (useServiceBind && _logger.debug())
			{
				_logger.debug("Fetching LDAP group permissions using the service-account connection: %s", r);
			}
			if (!lc.fetchGroupPermissions(r, permissions))
			{
				return false;
			}
			replacePermissionsForRoleImpl(r, permissions);
		}
		replaceRolesForUserImpl(username, roles);
	}

	return true;

#else
	throw Poco::NotImplementedException("LDAP is not supported by this build of OSP Auth. Rebuild with -DPOCO_OSP_AUTH_ENABLE_LDAP");
#endif
}


std::string AbstractLDAPAuthAdminService::escapeLDAP(const std::string& str)
{
	std::string result;
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
	{
		switch (*it)
		{
		case ',':
		case ';':
		case '+':
		case '#':
		case '\\':
		case '<':
		case '>':
		case '"':
		case '=':
			result += '\\';
			result += *it;
			break;
		default:
			result += *it;
			break;
		}
	}
	return result;
}


bool AbstractLDAPAuthAdminService::isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const
{
	for (auto it = _ldapParams.extraAttributes.begin(); it != _ldapParams.extraAttributes.end(); ++it)
	{
		if (Poco::UTF8::icompare(it->first, attr) == 0)
		{
			canonicalAttr = it->second;
			return true;
		}
	}
	return false;
}


} } } // namespace Poco::OSP::Auth
