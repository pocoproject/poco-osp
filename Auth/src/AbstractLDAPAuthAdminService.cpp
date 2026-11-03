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
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/AbstractLDAPAuthAdminService.h"
#include "Poco/OSP/Auth/AuthTypes.h"
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
	if (_ldapParams.uri.empty())
	{
		return;
	}
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	const std::string authMode = Poco::toUpper(_ldapParams.authMode);
	const std::string bindMode = Poco::toUpper(_ldapParams.bindMode);
	_accountMode = authMode == "ACCOUNT"s;
	if (!_accountMode && !authMode.empty() && authMode != "DN"s)
	{
		refuseLDAP("auth.ldap.authMode is \""s + _ldapParams.authMode + "\", which is neither DN nor ACCOUNT"s);
	}
	else if (_accountMode)
	{
		if (!bindMode.empty() && bindMode != "SERVICE"s && bindMode != "USER"s)
		{
			refuseLDAP("auth.ldap.bindMode is \""s + _ldapParams.bindMode + "\", which is neither SERVICE nor USER"s);
		}
		else
		{
			if (_ldapParams.bindUser.empty() && bindMode == "SERVICE"s)
			{
				_logger.warning("LDAP: auth.ldap.bindMode is SERVICE, but auth.ldap.bindUser is not set, so the USER "
					"bind mode is used: searches and reads use the account of the user who logs in.");
			}
			_serviceBind = !_ldapParams.bindUser.empty() && bindMode != "USER"s;
			if (_serviceBind)
			{
				_logger.information("LDAP: the ACCOUNT mode uses the SERVICE bind mode: searches and reads use "
					"auth.ldap.bindUser.");
			}
			else
			{
				_logger.information("LDAP: the ACCOUNT mode uses the USER bind mode: searches and reads use the "
					"account of the user who logs in.");
			}
		}
	}
	else
	{
		if (_ldapParams.bindDN.empty())
		{
			_logger.warning("LDAP: auth.ldap.bindDN is not set; the DN of a user is built from auth.ldap.searchBase. "
				"Set auth.ldap.bindDN to the DN of the user entries, with %s in place of the user name.");
		}
		const std::string& dnTemplate = _ldapParams.bindDN.empty() ? _ldapParams.searchBase : _ldapParams.bindDN;
		std::size_t placeholders = 0;
		for (auto pos = dnTemplate.find("%s"s); pos != std::string::npos; pos = dnTemplate.find("%s"s, pos + 2))
		{
			++placeholders;
		}
		if (placeholders != 1)
		{
			refuseLDAP((_ldapParams.bindDN.empty() ? "auth.ldap.searchBase"s : "auth.ldap.bindDN"s) + " is \""s +
				dnTemplate + "\", which does not contain %s exactly once in place of the user name"s);
		}
	}
	if (_serviceBind && _ldapParams.bindPassword.empty())
	{
		_logger.error("LDAP: auth.ldap.bindUser is set but auth.ldap.bindPassword is empty, so the LDAP server cannot "
			"verify passwords until auth.ldap.bindPassword is set.");
	}
	_pRoleFilter = compileLDAPExpression("auth.ldap.roleFilter"s, _ldapParams.roleFilter);
	_pLocalRoles = compileLDAPExpression("auth.ldap.localRoles"s, _ldapParams.localRoles);
	for (const auto& [ldapAttribute, userAttribute]: _ldapParams.extraAttributes)
	{
		if (isAuthAttribute(userAttribute))
		{
			_logger.error("LDAP: auth.ldap.extraAttributes maps %s to %s, a name reserved for the attributes of the "
				"authentication service, so its value is not stored."s, ldapAttribute, userAttribute);
		}
	}
	if (_ldapParams.extraAttributes.size() > LDAPClient::MAX_EXTRA_ATTRIBUTES)
	{
		_logger.warning("LDAP: auth.ldap.extraAttributes lists %z attributes; only the first %z are read from the LDAP "
			"server.", _ldapParams.extraAttributes.size(), LDAPClient::MAX_EXTRA_ATTRIBUTES);
	}
#else
	_logger.error("LDAP: auth.ldap.uri is set, but this build of OSP Auth does not support LDAP, so the LDAP server is "
		"not used.");
#endif
}


AbstractLDAPAuthAdminService::~AbstractLDAPAuthAdminService() = default;


bool AbstractLDAPAuthAdminService::authenticateLDAP(const std::string& username, const std::string& credentials,
	bool& fallbackLocal) const
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

	if (!_configurationError.empty())
	{
		throw Poco::IOException("The LDAP settings cannot be used"s, _configurationError);
	}

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
		client.setMode(_accountMode ? LDAPClient::Mode::ACCOUNT_NAME : LDAPClient::Mode::DIST_NAME);
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
	const bool useServiceBind = _serviceBind;

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
	if (!useServiceBind && !acceptAccount())
	{
		return LDAPResult::LOCKED_OUT;
	}

	for (const auto& r: lc.roles())
	{
		if (_pRoleFilter != nullptr && !_pRoleFilter->match(r))
		{
			if (_logger.debug())
			{
				_logger.debug("Skipping LDAP role due to roleFilter: %s", r);
			}
		}
		else if (_pLocalRoles != nullptr && _pLocalRoles->match(r))
		{
			if (_logger.debug())
			{
				_logger.debug("Skipping LDAP role %s: a local role has this name.", r);
			}
		}
		else if (_ldapParams.rolePermissionAttribute.empty())
		{
			user.roles[r] = std::nullopt;
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
	throw Poco::NotImplementedException("LDAP is not supported by this build of OSP Auth. "
		"Rebuild with -DENABLE_OSP_AUTH_LDAP=ON");
#endif
}


void AbstractLDAPAuthAdminService::storeLDAPUser(const std::string& username, const std::string& credentials,
	const LDAPUser& user) const
{
	std::map<std::string, std::optional<Permissions>> importedRoles;
	for (const auto& [role, permissions]: user.roles)
	{
		if (getRoleAttributeImpl(role, ROLE_ATTR_LDAP) == "false"s)
		{
			if (_logger.debug())
			{
				_logger.debug("Skipping LDAP role %s: a local role has this name.", role);
			}
		}
		else if (!permissions.has_value() && !_ldapParams.rolePermissionAttribute.empty())
		{
			throw Poco::IOException("Cannot read the permissions of the LDAP group", role);
		}
		else
		{
			importedRoles.emplace(role, permissions);
		}
	}

	createOrUpdateUserImpl(username, credentials);
	replaceUserAttributeImpl(username, USER_ATTR_FULL_NAME, user.fullName);

	if (!_ldapParams.permissionAttribute.empty())
	{
		replacePermissionsForUserImpl(username, user.permissions);
	}

	for (const auto& a: user.attributes)
	{
		if (!isAuthAttribute(a.first))
		{
			replaceUserAttributeImpl(username, a.first, a.second);
		}
	}

	if (!_ldapParams.roleAttribute.empty())
	{
		std::set<std::string> roles;
		for (const auto& [role, permissions]: importedRoles)
		{
			createOrUpdateRoleImpl(role);
			replaceRoleAttributeImpl(role, ROLE_ATTR_LDAP, "true");
			if (permissions.has_value())
			{
				replacePermissionsForRoleImpl(role, *permissions);
			}
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


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
void AbstractLDAPAuthAdminService::refuseLDAP(const std::string& reason)
{
	_logger.error("LDAP: %s, so the LDAP server is not used."s, reason);
	if (_configurationError.empty())
	{
		_configurationError = reason;
	}
}


std::unique_ptr<Poco::RegularExpression> AbstractLDAPAuthAdminService::compileLDAPExpression(
	const std::string& setting, const std::string& pattern)
{
	if (pattern.empty())
	{
		return nullptr;
	}
	try
	{
		return std::make_unique<Poco::RegularExpression>(pattern);
	}
	catch (Poco::RegularExpressionException& exc)
	{
		refuseLDAP(setting + " is not a valid regular expression ("s + exc.message() + ")"s);
		return nullptr;
	}
}
#endif


} // namespace Poco::OSP::Auth
