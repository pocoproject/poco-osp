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

#if defined(POCO_OS_FAMILY_WINDOWS)
#include "LDAPClient_WIN32.cpp"
#else
#include "LDAPClient_POSIX.cpp"
#endif

using namespace std::string_literals;


namespace Poco {
namespace OSP {
namespace Auth {


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
}


LDAPClient::LDAPClient(Poco::Logger& logger):
	_logger(logger)
{
	_impl = std::make_unique<LDAPClientImpl>(_logger);
}


LDAPClient::~LDAPClient() = default;


bool LDAPClient::initialize(const std::string& uri)
{
	return _impl->initialize(uri);
}


bool LDAPClient::setTimeLimit(int limit)
{
	return _impl->setTimeLimit(limit);
}


LDAPClient::Result LDAPClient::bind(const std::string& username, const std::string& credentials)
{
	auto credentialName { username };
	if (_mode == Mode::DIST_NAME)
	{
		credentialName = Poco::format(_searchBase, escapeLDAP(username));
	}
	_username = username;

	return _impl->bind(credentialName, credentials);
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
	LDAPClientImpl::AuthProperties properties;
	const auto filter = accountSearchFilter(_userObjectClass, username);

	if (_impl->find(_searchBase, filter, true, false, properties))
	{
		userDN = std::move(properties.distinguishedName);
		return !userDN.empty();
	}

	return false;
}


bool LDAPClient::fetchOwnProperties()
{
	LDAPClientImpl::AuthProperties properties;

	auto searchBase {_searchBase};
	auto username {_username};
	std::string filter;

	switch (_mode)
	{
	case Mode::DIST_NAME:
		searchBase = Poco::format(_searchBase, escapeLDAP(username));
		filter = "(objectClass=" + _userObjectClass + ")";
		break;
	case Mode::ACCOUNT_NAME:
		filter = accountSearchFilter(_userObjectClass, username);
		break;
	}

	if (_impl->find(searchBase, filter, _mode != Mode::DIST_NAME, false, properties))
	{
		_fullName = std::move(properties.fullName);
		_permissions = std::move(properties.permissions);
		_roles = std::move(properties.roles);
		_attributes = std::move(properties.attributes);
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

	LDAPClientImpl::AuthProperties properties;
	const std::string filter = "(objectClass=" + _userObjectClass + ")";

	if (_impl->find(userDN, filter, false, false, properties))
	{
		_fullName = std::move(properties.fullName);
		_permissions = std::move(properties.permissions);
		_roles = std::move(properties.roles);
		_attributes = std::move(properties.attributes);
		return true;
	}

	return false;
}


bool LDAPClient::fetchGroupPermissions(const std::string& groupDN, Permissions& permissions)
{
	LDAPClientImpl::AuthProperties properties;
	std::string filter = "(objectClass=" + _groupObjectClass + ")";

	if (_impl->find(groupDN, filter, false, true, properties))
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


std::string LDAPClient::escapeLDAP(const std::string& str)
{
	std::string result;
	result.reserve(str.size());
	for (auto ch: str)
	{
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
		default:
			result += ch;
			break;
		}
	}
	return result;
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


} } } // namespace Poco::OSP::Auth
