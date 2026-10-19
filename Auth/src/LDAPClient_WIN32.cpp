//
// LDAPClient_WIN32.cpp
//
// Library: OSP
// Package: Auth
// Module:  LDAPClient
//
// Definition of the LDAPClientImpl WIN32 class using WinLDAP.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/Logger.h"
#include "Poco/String.h"
#include "Poco/UTF8String.h"
#include "Poco/URI.h"
#include "Poco/NumberParser.h"
#include "Poco/UnicodeConverter.h"
#include "Poco/OSP/Auth/LDAPClient.h"

#include <set>
#include <vector>
#include <windows.h>
#include <winldap.h>
#include <winber.h>

using namespace std::string_literals;

namespace {
	// Convert wide LDAP error string to UTF-8 std::string
	std::string ldapError(ULONG err)
	{
		// ldap_err2stringW may return a wide string
		LPWSTR w = ldap_err2stringW(err);
		if (!w) return "Unknown LDAP error";
		std::wstring ws(w);
		std::string s;
		Poco::UnicodeConverter::toUTF8(ws, s);
		return s;
	}
}


namespace Poco {
namespace OSP {
namespace Auth {


class LDAPClientImpl
{
public:

	using Roles = LDAPClient::Roles;
	using Permissions = LDAPClient::Permissions;
	using Attributes = LDAPClient::Attributes;
	using AttributesNameMap = LDAPClient::AttributesNameMap;
	using Mode = LDAPClient::Mode;
	using Result = LDAPClient::Result;

	struct AuthProperties {
		std::string distinguishedName;
		std::string fullName;
		Roles roles;
		Permissions permissions;
		Attributes attributes;
	};

	LDAPClientImpl(Poco::Logger& logger);
		/// Windows LDAPClient implementation.

	~LDAPClientImpl();

	bool initialize(const std::string& uri);

	bool setTimeLimit(int limit);

	Result bind(const std::string& bindDN, const std::string& credentials);

	bool isBound() const;

	void unbind();

	void setTimeout(int secs);
		/// Query timeout

	void setRoleAttributeName(const std::string& name);
	void setPermissionAttributeName(const std::string& name);
	void setRolePermissionAttributeName(const std::string& name);
	void setExtraAttributesMap(const AttributesNameMap& nameMap);

	bool find(const std::string& searchBase, const std::string& filter, bool subtree, bool queryRoles, AuthProperties& properties);

	int lastReturnCode() const;
		/// Return code of the last LDAP operation.

private:

	bool isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const;

	static const std::string DISPLAY_NAME_ATTR;

	Poco::Logger& _logger;
	LDAP* _ldap{nullptr};
	bool _bindSuccessful{false};
	ULONG _rc{0};

	int _timeoutSec{30};

	std::string _roleAttributeName;
	std::string _permissionAttributeName;
	std::string _rolePermissionAttributeName;
	AttributesNameMap _extraAttributesMap;
};

const std::string LDAPClientImpl::DISPLAY_NAME_ATTR{"displayName"s};


LDAPClientImpl::LDAPClientImpl(Poco::Logger& logger) : _logger(logger)
{
}


LDAPClientImpl::~LDAPClientImpl()
{
	unbind();
}


bool LDAPClientImpl::initialize(const std::string& uri)
{
	try
	{
		// Parse the URI to extract host and port
		Poco::URI ldapUri(uri);
		std::string host = ldapUri.getHost();
		int port = ldapUri.getPort();

		// If no port specified, use default based on scheme
		if (port == 0)
		{
			if (ldapUri.getScheme() == "ldaps")
				port = LDAP_SSL_PORT;
			else
				port = LDAP_PORT;
		}

		// Convert host to wide string for WinLDAP
		std::wstring whost;
		Poco::UnicodeConverter::toUTF16(host, whost);

		if (ldapUri.getScheme() == "ldaps")
			_ldap = ldap_sslinitW(const_cast<LPWSTR>(whost.c_str()), static_cast<ULONG>(port), 1);
		else
			_ldap = ldap_initW(const_cast<LPWSTR>(whost.c_str()), static_cast<ULONG>(port));

		if (!_ldap)
		{
			_logger.error("Failed to initialize LDAP client for URI: %s"s, uri);
			return false;
		}

		ULONG version = LDAP_VERSION3;
		_rc = ldap_set_optionW(_ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
		if (_rc != LDAP_SUCCESS)
		{
			_logger.error("Failed to set LDAP version 3: %s"s, ldapError(_rc));
			ldap_unbind_s(_ldap);
			_ldap = nullptr;
			return false;
		}

		_logger.debug("LDAP initialized successfully: %s"s, uri);
		return true;
	}
	catch (const Poco::Exception& ex)
	{
		_logger.error("Failed to parse LDAP URI '%s': %s"s, uri, ex.displayText());
		return false;
	}
}


bool LDAPClientImpl::setTimeLimit(int limit)
{
	if (!_ldap) return false;

	_rc = ldap_set_optionW(_ldap, LDAP_OPT_TIMELIMIT, &limit);
	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Failed to set time limit: %s"s, ldapError(_rc));
		return false;
	}
	return true;
}


void LDAPClientImpl::setTimeout(int secs)
{
	_timeoutSec = secs;
}


LDAPClientImpl::Result LDAPClientImpl::bind(const std::string& bindDN, const std::string& credentials)
{
	Result result{Result::ERROR_RESULT};

	if (!_ldap)
	{
		_logger.error("LDAP not initialized");
		return Result::ERROR_RESULT;
	}

	if (credentials.empty())
	{
		// Anonymous bind is attempted if credentials (password) is ampty.
		_logger.warning("LDAP: anonymous bind with empty credentials (%s). Some operations may fail."s, bindDN);
	}

	_bindSuccessful = false;

	// Convert bindDN and credentials to wide strings
	std::wstring wbindDN;
	std::wstring wcreds;
	Poco::UnicodeConverter::toUTF16(bindDN, wbindDN);
	Poco::UnicodeConverter::toUTF16(credentials, wcreds);

	_rc = ldap_simple_bind_sW(_ldap, const_cast<LPWSTR>(wbindDN.c_str()), const_cast<LPWSTR>(wcreds.c_str()));
	
	switch (_rc)
	{
	case LDAP_SUCCESS:
		_logger.debug("LDAP bind successful: %s."s, bindDN);
		_bindSuccessful = true;
		result = Result::SUCCESS_RESULT;
		break;
	case LDAP_INVALID_CREDENTIALS:
		_logger.debug("LDAP bind failed (invalid credentials): %s."s, bindDN);
		result = Result::WRONG_CREDENTIALS;
		break;
	case LDAP_SERVER_DOWN:
	case LDAP_CONNECT_ERROR:
		_logger.error("LDAP server inaccessible for %s: %s"s, bindDN, ldapError(_rc));
		result = Result::SERVER_INACCESSIBLE;
		break;
	default:
		_logger.error("LDAP bind failed for %s: %s"s, bindDN, ldapError(_rc));
		result = Result::ERROR_RESULT;
	}
	return result;
}


bool LDAPClientImpl::isBound() const
{
	return _ldap != nullptr && _bindSuccessful;
}


void LDAPClientImpl::unbind()
{
	if (_ldap)
	{
		ULONG rc = ldap_unbind_s(_ldap);
		if (rc != LDAP_SUCCESS)
		{
			_logger.warning("LDAP unbind error (ignored): %s"s, ldapError(rc));
		}
		_ldap = nullptr;
		_bindSuccessful = false;
		_rc = 0;
	}
}


void LDAPClientImpl::setRoleAttributeName(const std::string& name)
{
	_roleAttributeName = name;
}


void LDAPClientImpl::setPermissionAttributeName(const std::string& name)
{
	_permissionAttributeName = name;
}


void LDAPClientImpl::setRolePermissionAttributeName(const std::string& name)
{
	_rolePermissionAttributeName = name;
}


void LDAPClientImpl::setExtraAttributesMap(const AttributesNameMap& nameMap)
{
	_extraAttributesMap = nameMap;
}


bool LDAPClientImpl::find(const std::string& searchBase, const std::string& filter, bool subtree, bool queryRoles, AuthProperties& properties)
{
	if (!_ldap)
	{
		_logger.error("LDAP not initialized");
		return false;
	}

	constexpr int MAX_LDAP_ATTRS = 12;
	LPWSTR pSearchAttr[MAX_LDAP_ATTRS + 1];
	int nSearchAttr = 0;

	const auto& permAttrName{ queryRoles ? _rolePermissionAttributeName : _permissionAttributeName };

	// Convert attribute names to wide strings on the fly and store pointers.
	// We'll allocate temporary wstrings to hold them and keep pointers valid for the call.
	std::vector<std::wstring> tmpAttrs;
	auto pushAttr = [&](const std::string& a)
	{
		tmpAttrs.emplace_back();
		Poco::UnicodeConverter::toUTF16(a, tmpAttrs.back());
		pSearchAttr[nSearchAttr++] = const_cast<LPWSTR>(tmpAttrs.back().c_str());
	};

	// displayName
	pushAttr(DISPLAY_NAME_ATTR);
	// permission or role-permission attribute
	if (!permAttrName.empty()) pushAttr(permAttrName);
	// role attribute
	if (!_roleAttributeName.empty()) pushAttr(_roleAttributeName);
	// extra attributes
	for (auto it = _extraAttributesMap.begin(); nSearchAttr < MAX_LDAP_ATTRS && it != _extraAttributesMap.end(); ++it)
	{
		pushAttr(it->first);
	}
	pSearchAttr[nSearchAttr] = nullptr;

	LDAPMessage* pMessage = nullptr;

	// Build wide variants of searchBase and filter
	std::wstring wSearchBase;
	std::wstring wFilter;
	Poco::UnicodeConverter::toUTF16(searchBase, wSearchBase);
	Poco::UnicodeConverter::toUTF16(filter, wFilter);

	// Setup timeout (l_timeval used by WinLDAP)
	l_timeval timeout;
	timeout.tv_sec = _timeoutSec;
	timeout.tv_usec = 0;

	_rc = ldap_search_ext_sW(
		_ldap,
		const_cast<LPWSTR>(wSearchBase.c_str()),
		subtree ? LDAP_SCOPE_SUBTREE : LDAP_SCOPE_BASE,
		const_cast<LPWSTR>(wFilter.c_str()),
		pSearchAttr,
		0,
		nullptr,
		nullptr,
		&timeout,
		1000,
		&pMessage
	);

	_logger.debug("Search \"%s\" \"%s\" attribute count %d: rc = %d"s, searchBase, filter, nSearchAttr, _rc);
	for (int i{0}; i < nSearchAttr; i++)
	{
		// convert attr back to UTF-8 for logging
		std::string attrUtf8;
		Poco::UnicodeConverter::toUTF8(tmpAttrs[i], attrUtf8);
		_logger.debug("LDAP query attr: %s"s, attrUtf8);
	}

	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("LDAP search failed: %s"s, ldapError(_rc));
		if (pMessage)
		{
			ldap_msgfree(pMessage);
		}
		return false;
	}

	std::string displayName;
	std::string distinguishedName;
	Permissions ldapPermissions;
	Roles ldapRoles;
	Attributes ldapAttributes;
	int entryCount = 0;

	for (LDAPMessage* pEntry = ldap_first_entry(_ldap, pMessage); pEntry != nullptr; pEntry = ldap_next_entry(_ldap, pEntry))
	{
		entryCount++;
		LPWSTR pDN = ldap_get_dnW(_ldap, pEntry);
		if (pDN != nullptr)
		{
			try
			{
				std::wstring wdn(pDN);
				Poco::UnicodeConverter::toUTF8(wdn, distinguishedName);
			}
			catch (...)
			{
				ldap_memfreeW(pDN);
				throw;
			}
			ldap_memfreeW(pDN);
		}
		BerElement* pBer = nullptr;

		for (LPWSTR pAttr = ldap_first_attributeW(_ldap, pEntry, &pBer); pAttr != nullptr; pAttr = ldap_next_attributeW(_ldap, pEntry, pBer))
		{
			// pAttr is wide string
			LPWSTR* ppValues = ldap_get_valuesW(_ldap, pEntry, pAttr);
			if (ppValues)
			{
				for (int i = 0; ppValues[i] != nullptr; ++i)
				{
					// convert value to UTF-8
					std::wstring wvalue(ppValues[i]);
					std::string value;
					Poco::UnicodeConverter::toUTF8(wvalue, value);
					Poco::trimInPlace(value);

					// convert attribute name to UTF-8 for comparisons/lookup
					std::wstring wattr(pAttr);
					std::string attr;
					Poco::UnicodeConverter::toUTF8(wattr, attr);

					std::string canonicalAttr;

					if (Poco::UTF8::icompare(DISPLAY_NAME_ATTR, attr) == 0)
					{
						if (_logger.debug())
						{
							_logger.debug("Found display name: \"%s\""s, value);
						}
						displayName = value;
					}
					else if (Poco::UTF8::icompare(permAttrName, attr) == 0)
					{
						if (_logger.debug())
						{
							_logger.debug("Found permission: \"%s\""s, value);
						}
						ldapPermissions.insert(value);
					}
					else if (Poco::UTF8::icompare(_permissionAttributeName, attr) == 0)
					{
						if (_logger.debug())
						{
							_logger.debug("Found permission: \"%s\""s, value);
						}
						ldapPermissions.insert(value);
					}
					else if (Poco::UTF8::icompare(_rolePermissionAttributeName, attr) == 0)
					{
						if (_logger.debug())
						{
							_logger.debug("Found permission: \"%s\""s, value);
						}
						ldapPermissions.insert(value);
					}
					else if (Poco::UTF8::icompare(_roleAttributeName, attr) == 0)
					{
						if (_logger.debug())
						{
							_logger.debug("Found role: \"%s\""s, value);
						}
						ldapRoles.insert(Poco::toLower(value));
					}
					else if (isExtraAttribute(attr, canonicalAttr))
					{
						if (_logger.debug())
						{
							_logger.debug("Found attribute: \"%s\" = \"%s\""s, attr, value);
						}
						ldapAttributes[canonicalAttr] = value;
					}
				}
				ldap_value_freeW(ppValues);
			}

			// Free attribute string (wide)
			ldap_memfreeW(pAttr);
		}

		if (pBer)
		{
			ber_free(pBer, 0);
		}
	}

	ldap_msgfree(pMessage);

	if (entryCount < 1)
	{
		if (!queryRoles)
		{
			_logger.warning("LDAP: record not found for: \"%s\", \"%s\""s, searchBase, filter);
			return false;
		}
		_logger.debug("LDAP: record not found for: \"%s\", \"%s\""s, searchBase, filter);
	}
	else if (entryCount > 1)
	{
		_logger.warning("LDAP: too many records for: \"%s\", \"%s\": %d"s, searchBase, filter, entryCount);
		return false;
	}

	properties.distinguishedName = std::move(distinguishedName);
	properties.fullName = displayName;
	properties.permissions = std::move(ldapPermissions);
	properties.roles = std::move(ldapRoles);
	properties.attributes = std::move(ldapAttributes);

	return true;
}


int LDAPClientImpl::lastReturnCode() const
{
	return static_cast<int>(_rc);
}


bool LDAPClientImpl::isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const
{
	for (const auto& el : _extraAttributesMap)
	{
		if (Poco::UTF8::icompare(el.first, attr) == 0)
		{
			canonicalAttr = el.second;
			return true;
		}
	}
	return false;
}

} } } // namespace Poco::OSP::Auth
