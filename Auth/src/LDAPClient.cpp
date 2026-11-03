//
// LDAPClient.cpp
//
// Library: OSP
// Package: Auth
// Module:  LDAPClient
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/LDAPClient.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include "Poco/Timespan.h"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#if defined(POCO_OS_FAMILY_WINDOWS)
#include "LDAPClient_WIN32.cpp"
#else
#include "LDAPClient_POSIX.cpp"
#endif

using namespace std::string_literals;


namespace Poco::OSP::Auth {


namespace
{
	std::string escapeHexByte(unsigned char value)
	{
		constexpr char HEX[] = "0123456789ABCDEF";

		std::string result;
		result += '\\';
		result += HEX[(value >> 4) & 0x0F];
		result += HEX[value & 0x0F];
		return result;
	}


	std::string normalizeAccountName(const std::string& username, std::string& upn)
	{
		std::string accountName{username};

		const auto pos = accountName.find('\\');
		if (pos != std::string::npos)
		{
			accountName = accountName.substr(pos + 1);
		}

		upn = accountName;
		const auto at = accountName.find('@');
		if (at != std::string::npos)
		{
			accountName = accountName.substr(0, at);
		}

		return accountName;
	}


	std::string accountSearchFilter(const std::string& userObjectClass, const std::string& username)
	{
		std::string upn;
		const auto accountName = normalizeAccountName(username, upn);
		return Poco::format(
			"(&(objectClass=%s)(|(sAMAccountName=%s)(userPrincipalName=%s)))"s,
			userObjectClass,
			LDAPClient::escapeFilterValue(accountName),
			LDAPClient::escapeFilterValue(upn)
		);
	}


	[[nodiscard]] std::string onlyAccountName(std::vector<std::string>& accountNames, const std::string& name)
		/// Returns the only sAMAccountName value of the entry of the given
		/// user or DN, or an empty string if it has none.
	{
		if (accountNames.size() > 1)
		{
			throw Poco::IOException("The LDAP entry of the user has more than one sAMAccountName value", name);
		}
		return accountNames.empty() ? std::string() : std::move(accountNames.front());
	}


	[[nodiscard]] std::string onlyValue(std::vector<std::string>& values)
		/// Returns the only value of an attribute of an entry, or an empty
		/// string if it has none or several.
	{
		return values.size() == 1 ? std::move(values.front()) : std::string();
	}
}


LDAPClient::LDAPClient(Poco::Logger& logger):
	_logger(logger)
{
	_impl = std::make_unique<LDAPClientImpl>(_logger);
}


LDAPClient::~LDAPClient() = default;


LDAPClient::Result LDAPClient::bindResult(int resultCode)
{
	switch (resultCode)
	{
	case LDAP_SUCCESS:
		return Result::SUCCESS_RESULT;
	case LDAP_INVALID_CREDENTIALS:
	case LDAP_INAPPROPRIATE_AUTH:
#if defined(POCO_OS_FAMILY_WINDOWS)
	case LDAP_INSUFFICIENT_RIGHTS:
#else
	case LDAP_INSUFFICIENT_ACCESS:
#endif
	case LDAP_UNWILLING_TO_PERFORM:
	case LDAP_CONSTRAINT_VIOLATION:
		return Result::WRONG_CREDENTIALS;
	case LDAP_BUSY:
	case LDAP_UNAVAILABLE:
	case LDAP_TIMELIMIT_EXCEEDED:
	case LDAP_SERVER_DOWN:
	case LDAP_CONNECT_ERROR:
	case LDAP_TIMEOUT:
		return Result::SERVER_INACCESSIBLE;
	default:
		return Result::ERROR_RESULT;
	}
}


bool LDAPClient::initialize(const std::string& uri)
{
	return _impl->initialize(uri);
}


bool LDAPClient::setTimeLimit(int limit)
{
	return _impl->setTimeLimit(limit);
}


bool LDAPClient::setTimeout(const Poco::Timespan& timeout)
{
	return _impl->setTimeout(timeout);
}


LDAPClient::Result LDAPClient::bind(const std::string& username, const std::string& credentials)
{
	if (!isValidBindValue(username))
	{
		_logger.debug("LDAP bind refused without contacting the server: the user name is empty or contains a NUL "
			"character."s);
		return Result::WRONG_CREDENTIALS;
	}

	auto credentialName { username };
	if (_mode == Mode::DIST_NAME)
	{
		credentialName = distinguishedName(username);
	}
	_username = username;

	return bindDirect(credentialName, credentials);
}


LDAPClient::Result LDAPClient::bindDirect(const std::string& bindName, const std::string& credentials)
{
	return _impl->bind(bindName, credentials);
}


bool LDAPClient::isBound() const
{
	return _impl->isBound();
}


void LDAPClient::unbind()
{
	_impl->unbind();
}


void LDAPClient::setMode(Mode mode)
{
	_mode = mode;
}


LDAPClient::Mode LDAPClient::mode() const
{
	return _mode;
}


void LDAPClient::setRoleAttributeName(const std::string& name)
{
	_impl->setRoleAttributeName(name);
}


void LDAPClient::setPermissionAttributeName(const std::string& name)
{
	_impl->setPermissionAttributeName(name);
}


void LDAPClient::setRolePermissionAttributeName(const std::string& name)
{
	_impl->setRolePermissionAttributeName(name);
}


void LDAPClient::setExtraAttributesMap(const AttributesNameMap& nameMap)
{
	_impl->setExtraAttributesMap(nameMap);
}


bool LDAPClient::findUserDN(const std::string& username, std::string& userDN)
{
	_accountName.clear();
	_userPrincipalName.clear();
	_principalName.clear();
	if (!isValidBindValue(username))
	{
		return false;
	}

	LDAPClientImpl::AuthProperties properties;
	const auto filter = accountSearchFilter(_userObjectClass, username);

	// Searched like a group, for which finding no entry is not a failure,
	// so that only a failed search and several entries are reported as such.
	if (!_impl->find(_searchBase, filter, true, true, _mode == Mode::ACCOUNT_NAME, properties))
	{
		throw Poco::IOException("The LDAP search for the user failed or found more than one entry", username);
	}
	_accountName = onlyAccountName(properties.accountNames, username);
	_userPrincipalName = onlyValue(properties.userPrincipalNames);
	_principalName = onlyValue(properties.principalNames);
	userDN = std::move(properties.distinguishedName);
	return !userDN.empty();
}


bool LDAPClient::fetchOwnProperties()
{
	_accountName.clear();
	_userPrincipalName.clear();
	_principalName.clear();
	LDAPClientImpl::AuthProperties properties;

	auto searchBase {_searchBase};
	auto username {_username};
	std::string filter;

	switch (_mode)
	{
	case Mode::DIST_NAME:
		searchBase = distinguishedName(username);
		filter = "(objectClass=" + _userObjectClass + ")";
		break;
	case Mode::ACCOUNT_NAME:
		filter = accountSearchFilter(_userObjectClass, username);
		break;
	}

	if (_impl->find(searchBase, filter, _mode != Mode::DIST_NAME, false, _mode == Mode::ACCOUNT_NAME, properties))
	{
		_accountName = onlyAccountName(properties.accountNames, username);
		_userPrincipalName = onlyValue(properties.userPrincipalNames);
		_principalName = onlyValue(properties.principalNames);
		setOwnProperties(
			std::move(properties.fullName),
			std::move(properties.roles),
			std::move(properties.permissions),
			std::move(properties.attributes));
		return true;
	}

	return false;
}


bool LDAPClient::fetchOwnProperties(const std::string& userDN)
{
	if (userDN.empty())
	{
		return fetchOwnProperties();
	}

	_accountName.clear();
	_userPrincipalName.clear();
	_principalName.clear();
	LDAPClientImpl::AuthProperties properties;
	const std::string filter = "(objectClass=" + _userObjectClass + ")";

	if (_impl->find(userDN, filter, false, false, _mode == Mode::ACCOUNT_NAME, properties))
	{
		_accountName = onlyAccountName(properties.accountNames, userDN);
		_userPrincipalName = onlyValue(properties.userPrincipalNames);
		_principalName = onlyValue(properties.principalNames);
		setOwnProperties(
			std::move(properties.fullName),
			std::move(properties.roles),
			std::move(properties.permissions),
			std::move(properties.attributes));
		return true;
	}

	return false;
}


bool LDAPClient::fetchGroupPermissions(const std::string& groupDN, Permissions& permissions)
{
	LDAPClientImpl::AuthProperties properties;
	std::string filter = "(objectClass=" + _groupObjectClass + ")";

	if (_impl->find(groupDN, filter, false, true, false, properties))
	{
		permissions = std::move(properties.permissions);
		return true;
	}
	return false;
}


int LDAPClient::lastReturnCode() const
{
	return _impl->lastReturnCode();
}


std::string LDAPClient::distinguishedName(const std::string& username) const
{
	return Poco::format(_bindDN.empty() ? _searchBase : _bindDN, escapeLDAP(username));
}


void LDAPClient::setOwnProperties(std::string fullName, Roles roles, Permissions permissions, Attributes attributes)
{
	_fullName = std::move(fullName);
	_roles = std::move(roles);
	_permissions = std::move(permissions);
	_attributes = std::move(attributes);
}


void LDAPClient::setAccountName(std::string accountName)
{
	_accountName = std::move(accountName);
}


void LDAPClient::setUserPrincipalName(std::string userPrincipalName)
{
	_userPrincipalName = std::move(userPrincipalName);
}


void LDAPClient::setPrincipalName(std::string principalName)
{
	_principalName = std::move(principalName);
}


std::string LDAPClient::escapeLDAP(const std::string& str)
{
	std::string result;
	result.reserve(str.size());
	for (std::size_t i = 0; i < str.size(); ++i)
	{
		const char ch = str[i];
		switch (ch)
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
			result += ch;
			break;
		case ' ':
			// A server may drop a leading or trailing space that is not escaped.
			if (i == 0 || i + 1 == str.size())
			{
				result += '\\';
			}
			result += ch;
			break;
		case '\0':
			result += escapeHexByte(0);
			break;
		default:
			result += ch;
			break;
		}
	}
	return result;
}


bool LDAPClient::isValidBindValue(const std::string& value)
{
	return !value.empty() && value.find('\0') == std::string::npos;
}


std::string LDAPClient::escapeFilterValue(const std::string& str)
{
	std::string result;
	result.reserve(str.size());
	for (unsigned char ch: str)
	{
		switch (ch)
		{
		case '*':
		case '(':
		case ')':
		case '\\':
		case '\0':
			result += escapeHexByte(ch);
			break;
		default:
			result += static_cast<char>(ch);
			break;
		}
	}
	return result;
}


} // namespace Poco::OSP::Auth
