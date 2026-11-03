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
#include "Poco/Exception.h"
#include "Poco/Bugcheck.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"
#include "Poco/Types.h"

#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	#include "Poco/OSP/Auth/LDAPClient.h"
#endif

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

using namespace std::string_literals;


namespace Poco::OSP::Auth {


AbstractLDAPAuthAdminService::AbstractLDAPAuthAdminService(Poco::Logger& logger, const LDAPParams& ldapParams):
	_logger(logger),
	_ldapParams(ldapParams)
{
	if (_ldapParams.timeout <= 0)
	{
		if (!_ldapParams.uri.empty())
		{
			_logger.warning("LDAP: auth.ldap.timeout is %d, so the default of %d seconds is used. Without a timeout, a "
				"bind to an LDAP server that does not answer would block every later login of the user.",
				_ldapParams.timeout, LDAPParams::DEFAULT_TIMEOUT);
		}
		_ldapParams.timeout = LDAPParams::DEFAULT_TIMEOUT;
	}
	const bool serviceBind = _ldapParams.authMode == "ACCOUNT"s && !_ldapParams.bindUser.empty() &&
		Poco::toUpper(_ldapParams.bindMode) != "USER"s;
	if (!_ldapParams.uri.empty() && _ldapParams.authMode == "DN"s && _ldapParams.bindDN.empty())
	{
		_logger.warning("LDAP: auth.ldap.bindDN is not set; the DN of a user is built from auth.ldap.searchBase. Set "
			"auth.ldap.bindDN to the DN of the user entries, with %s in place of the user name.");
	}
	if (!_ldapParams.uri.empty() && serviceBind && _ldapParams.bindPassword.empty())
	{
		_logger.error("LDAP: auth.ldap.bindUser is set but auth.ldap.bindPassword is empty, so the LDAP server cannot "
			"verify passwords until auth.ldap.bindPassword is set. Until then, LDAP users can log in only with the "
			"stored copy of their password, where auth.ldap.inaccessible.fallbackToLocal and "
			"auth.ldap.cache.lifetimeDays allow it.");
	}
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	if (!_ldapParams.uri.empty() && _ldapParams.extraAttributes.size() > LDAPClient::MAX_EXTRA_ATTRIBUTES)
	{
		_logger.warning("LDAP: auth.ldap.extraAttributes lists %z attributes; only the first %z are read from the LDAP "
			"server.", _ldapParams.extraAttributes.size(), LDAPClient::MAX_EXTRA_ATTRIBUTES);
	}
#endif
}


AbstractLDAPAuthAdminService::~AbstractLDAPAuthAdminService() = default;


bool AbstractLDAPAuthAdminService::authenticateLDAP(const std::string& username, const std::string& credentials, bool& fallbackLocal) const
{
	fallbackLocal = false;
	LDAPUser user;
	switch (verifyLDAP(username, credentials, user))
	{
	case LDAPResult::ACCEPTED:
		storeLDAPUser(user.accountName.empty() ? username : Poco::toLower(user.accountName), credentials, user);
		return true;
	case LDAPResult::INACCESSIBLE:
		fallbackLocal = canUseLocalCredentials(username);
		return false;
	case LDAPResult::REJECTED:
	case LDAPResult::REFUSED:
	case LDAPResult::LOCKED_OUT:
		return false;
	default:
		poco_bugcheck();
	}
}


AbstractLDAPAuthAdminService::LDAPResult AbstractLDAPAuthAdminService::verifyLDAP(
	[[maybe_unused]] const std::string& username, [[maybe_unused]] const std::string& credentials,
	[[maybe_unused]] LDAPUser& user) const
{
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)

	if (!LDAPClient::isValidBindValue(username) || !LDAPClient::isValidBindValue(credentials))
	{
		_logger.debug("LDAP authentication refused: the user name or the password is empty or contains a NUL "
			"character."s);
		return LDAPResult::REJECTED;
	}

	std::unique_ptr<LDAPClient> pClient = createLDAPClient();
	LDAPClient& lc = *pClient;

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

	auto initializeConnection = [this](LDAPClient& client) -> LDAPResult
	{
		if (!client.initialize(_ldapParams.uri))
		{
			return LDAPResult::INACCESSIBLE;
		}
		if (!client.setTimeLimit(_ldapParams.timeout) || !client.setTimeout(Poco::Timespan(_ldapParams.timeout, 0)))
		{
			throw Poco::IOException("Cannot set the timeouts of the LDAP connection"s, _ldapParams.uri);
		}
		return LDAPResult::ACCEPTED;
	};

	std::string userDN;
	std::string creds(credentials);
	// Clears the copy of the password on every return, also on an exception.
	const auto clearCreds = [](std::string* pCreds) { Poco::secureClear(*pCreds); };
	const std::unique_ptr<std::string, decltype(clearCreds)> pCredsGuard(&creds, clearCreds);
	LDAPClient::Result result = LDAPClient::Result::ERROR_RESULT;
	int bindCode = 0;
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

	// The account name, or the given name if the entry has none, so that the
	// check covers every login.
	const auto acceptAccount = [this, &username, &user]()
	{
		return acceptLDAPAccount(username, Poco::toLower(user.accountName.empty() ? username : user.accountName));
	};

	if (useServiceBind && _ldapParams.bindPassword.empty())
	{
		throw Poco::IOException("The LDAP service account cannot be used because auth.ldap.bindPassword is empty"s,
			_ldapParams.bindUser);
	}

	const LDAPResult connection = initializeConnection(lc);
	if (connection != LDAPResult::ACCEPTED)
	{
		return connection;
	}

	if (useServiceBind)
	{
		std::string bindCreds(_ldapParams.bindPassword);
		const auto bindResult = lc.bindDirect(_ldapParams.bindUser, bindCreds);
		Poco::secureClear(bindCreds);

		if (bindResult != LDAPClient::Result::SUCCESS_RESULT)
		{
			if (bindResult == LDAPClient::Result::SERVER_INACCESSIBLE) return LDAPResult::INACCESSIBLE;
			// The service account, not the user, is rejected.
			throw Poco::IOException("The LDAP server rejects the service account auth.ldap.bindUser"s,
				_ldapParams.bindUser);
		}

		if (!lc.findUserDN(username, userDN))
		{
			return LDAPResult::REJECTED;
		}
		user.accountName = lc.accountName();
		if (!acceptAccount())
		{
			return LDAPResult::LOCKED_OUT;
		}

		std::unique_ptr<LDAPClient> pVerifyClient = createLDAPClient();
		configureClient(*pVerifyClient);
		const LDAPResult verifyConnection = initializeConnection(*pVerifyClient);
		if (verifyConnection != LDAPResult::ACCEPTED)
		{
			return verifyConnection;
		}

		result = pVerifyClient->bindDirect(userDN, creds);
		bindCode = pVerifyClient->lastReturnCode();
	}
	else
	{
		result = lc.bind(username, creds);
		bindCode = lc.lastReturnCode();
	}

	switch (result)
	{
	case LDAPClient::Result::SUCCESS_RESULT:
		break;
	case LDAPClient::Result::WRONG_CREDENTIALS:
		return LDAPResult::REJECTED;
	case LDAPClient::Result::SERVER_INACCESSIBLE:
		return LDAPResult::INACCESSIBLE;
	default:
		_logger.warning("The LDAP server answers the bind of user %s with result code %d, which neither accepts nor "
			"rejects the password, so the login is refused."s, username, bindCode);
		return LDAPResult::REFUSED;
	}
	user.passwordVerified = true;

	if (useServiceBind && _logger.debug())
	{
		_logger.debug("Fetching LDAP user properties using the service-account connection.");
	}
	// The password has been verified by the bind, so a failure to read the directory
	// must not count as a rejected password.
	if (!lc.fetchOwnProperties(userDN))
	{
		throw Poco::IOException("Cannot read the LDAP entry of the user after a successful bind", username);
	}

	if (_logger.debug())
	{
		_logger.debug("Display name: %s", lc.fullName());
		for (const auto& p: lc.permissions())
		{
			_logger.debug("Permission: %s", p);
		}
		for (const auto& r: lc.roles())
		{
			_logger.debug("Role: %s", r);
		}
		for (const auto& a: lc.attributes())
		{
			_logger.debug("Attribute: %s = %s", a.first, a.second);
		}
	}

	// The USER bind mode binds with the given name, which the server may accept
	// for another identity than the entry found by that name, such as one of
	// another domain, so the entry is used only if it has the given name.
	if (lc.mode() == LDAPClient::Mode::ACCOUNT_NAME && !useServiceBind)
	{
		std::string entryName = lc.accountName();
		if (username.find('\\') != std::string::npos)
		{
			entryName = lc.principalName();
		}
		else if (username.find('@') != std::string::npos)
		{
			entryName = lc.userPrincipalName();
		}
		if (Poco::UTF8::icompare(username, entryName) != 0)
		{
			_logger.warning("The LDAP server accepts the bind of %s, but the entry found for that name does not have "
				"it as one of its names, so the login is refused."s, username);
			return LDAPResult::REFUSED;
		}
	}

	user.fullName = lc.fullName();
	// The read after the bind may return no account name; in the SERVICE bind
	// mode, the one that the search has read before the bind is kept.
	if (!useServiceBind || !lc.accountName().empty())
	{
		user.accountName = lc.accountName();
	}
	user.permissions = lc.permissions();
	user.attributes = lc.attributes();
	user.hasRoles = !lc.roles().empty();
	if (!useServiceBind && !acceptAccount())
	{
		return LDAPResult::LOCKED_OUT;
	}

	std::unique_ptr<Poco::RegularExpression> pRoleFilter;
	if (!_ldapParams.roleFilter.empty())
	{
		pRoleFilter = std::make_unique<Poco::RegularExpression>(_ldapParams.roleFilter);
	}
	std::unique_ptr<Poco::RegularExpression> pLocalRoles;
	if (!_ldapParams.localRoles.empty())
	{
		pLocalRoles = std::make_unique<Poco::RegularExpression>(_ldapParams.localRoles);
	}
	for (const auto& r: lc.roles())
	{
		if (pRoleFilter != nullptr && !pRoleFilter->match(r))
		{
			if (_logger.debug())
			{
				_logger.debug("Skipping LDAP role due to roleFilter: %s", r);
			}
		}
		else if (pLocalRoles != nullptr && pLocalRoles->match(r))
		{
			if (_logger.debug())
			{
				_logger.debug("Skipping LDAP role %s: a local role has this name.", r);
			}
		}
		else
		{
			if (useServiceBind && _logger.debug())
			{
				_logger.debug("Fetching LDAP group permissions using the service-account connection: %s", r);
			}
			LDAPClient::Permissions permissions;
			if (lc.fetchGroupPermissions(r, permissions))
			{
				user.roles[r] = std::move(permissions);
			}
			else
			{
				user.roles[r] = std::nullopt;
			}
		}
	}

	return LDAPResult::ACCEPTED;

#else
	throw Poco::NotImplementedException("LDAP is not supported by this build of OSP Auth. Rebuild with -DPOCO_OSP_AUTH_ENABLE_LDAP");
#endif
}


void AbstractLDAPAuthAdminService::storeLDAPUser(const std::string& username, const std::string& credentials,
	const LDAPUser& user) const
{
	std::map<std::string, Permissions> rolePermissions;
	for (const auto& [role, permissions]: user.roles)
	{
		if (getRoleAttributeImpl(role, ROLE_ATTR_LDAP) == "false"s)
		{
			if (_logger.debug())
			{
				_logger.debug("Skipping LDAP role %s: a local role has this name.", role);
			}
		}
		else if (!permissions.has_value())
		{
			throw Poco::IOException("Cannot read the permissions of the LDAP group", role);
		}
		else
		{
			rolePermissions[role] = *permissions;
		}
	}

	createOrUpdateUserImpl(username, credentials);
	replaceUserAttributeImpl(username, USER_ATTR_FULL_NAME, user.fullName);

	if (!user.permissions.empty())
	{
		replacePermissionsForUserImpl(username, user.permissions);
	}

	for (const auto& a: user.attributes)
	{
		replaceUserAttributeImpl(username, a.first, a.second);
	}

	if (user.hasRoles)
	{
		std::set<std::string> roles;
		for (const auto& [role, permissions]: rolePermissions)
		{
			createOrUpdateRoleImpl(role);
			replaceRoleAttributeImpl(role, ROLE_ATTR_LDAP, "true");
			replacePermissionsForRoleImpl(role, permissions);
			roles.insert(role);
		}
		replaceRolesForUserImpl(username, roles);
	}
}


bool AbstractLDAPAuthAdminService::acceptLDAPAccount(const std::string&, const std::string&) const
{
	return true;
}


bool AbstractLDAPAuthAdminService::canUseLocalCredentials(const std::string& username) const
{
	if (!_ldapParams.localFallback)
	{
		return false;
	}
	const auto lastTimeStr = getUserAttributeImpl(username, USER_ATTR_SUCCESS_TIME);
	if (lastTimeStr.empty())
	{
		return false;
	}
	Poco::Int64 lastTime = 0;
	if (!Poco::NumberParser::tryParse64(lastTimeStr, lastTime))
	{
		return false;
	}
	const Poco::Timespan lifetime(_ldapParams.uNameLifeDaysDays, 0, 0, 0, 0);
	if (!Poco::Timestamp::fromEpochTime(static_cast<std::time_t>(lastTime)).isElapsed(lifetime.totalMicroseconds()))
	{
		_logger.information("LDAP cannot be used, so the stored credentials of user %s are checked.", username);
		return true;
	}
	return false;
}


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
std::unique_ptr<LDAPClient> AbstractLDAPAuthAdminService::createLDAPClient() const
{
	return std::make_unique<LDAPClient>(_logger);
}
#endif


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


} // namespace Poco::OSP::Auth
