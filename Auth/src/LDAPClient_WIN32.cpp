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

#include <algorithm>
#include <memory>
#include <set>
#include <vector>
#include <windows.h>
#include <winldap.h>
#include <winber.h>

using namespace std::string_literals;

namespace {
	// Convert wide LDAP error string to UTF-8 std::string
	[[nodiscard]] std::string ldapError(ULONG err)
	{
		// ldap_err2stringW may return a wide string
		LPWSTR w = ldap_err2stringW(err);
		if (w == nullptr) return "Unknown LDAP error";
		std::wstring ws(w);
		std::string s;
		Poco::UnicodeConverter::toUTF8(ws, s);
		return s;
	}


	[[nodiscard]] l_timeval toTimeval(const Poco::Timespan& timespan)
	{
		l_timeval tv;
		tv.tv_sec = static_cast<LONG>(timespan.totalSeconds());
		tv.tv_usec = static_cast<LONG>(timespan.useconds());
		return tv;
	}


	struct MessageDeleter
	{
		void operator()(LDAPMessage* pMessage) const
		{
			ldap_msgfree(pMessage);
		}
	};


	struct BerElementDeleter
	{
		void operator()(BerElement* pBerElement) const
		{
			ber_free(pBerElement, 0);
		}
	};
}


namespace Poco::OSP::Auth {


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
		std::vector<std::string> accountNames;
			/// The values of sAMAccountName, if it is queried.
		std::vector<std::string> userPrincipalNames;
			/// The values of userPrincipalName, queried with sAMAccountName.
		std::vector<std::string> principalNames;
			/// The values of msDS-PrincipalName, queried with sAMAccountName.
	};

	LDAPClientImpl(Poco::Logger& logger);
		/// Windows LDAPClient implementation.

	~LDAPClientImpl();

	[[nodiscard]] bool initialize(const std::string& uri);

	[[nodiscard]] bool setTimeLimit(int limit);

	[[nodiscard]] Result bind(const std::string& bindDN, const std::string& credentials);

	[[nodiscard]] bool isBound() const;

	void unbind();

	[[nodiscard]] bool setTimeout(const Poco::Timespan& timeout);
		/// Sets the timeout for connecting, for sending each request and
		/// for the result of each operation.

	void setRoleAttributeName(const std::string& name);
	void setPermissionAttributeName(const std::string& name);
	void setRolePermissionAttributeName(const std::string& name);
	void setExtraAttributesMap(const AttributesNameMap& nameMap);

	[[nodiscard]] bool find(const std::string& searchBase, const std::string& filter, bool subtree, bool queryRoles,
		bool queryAccountName, AuthProperties& properties);

	[[nodiscard]] int lastReturnCode() const;
		/// Return code of the last LDAP operation.

private:

	[[nodiscard]] bool isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const;

	static const std::string DISPLAY_NAME_ATTR;
	static const std::string ACCOUNT_NAME_ATTR;
	static const std::string USER_PRINCIPAL_NAME_ATTR;
	static const std::string PRINCIPAL_NAME_ATTR;
	static constexpr long DEFAULT_TIMEOUT_SECONDS = 30;

	Poco::Logger& _logger;
	LDAP* _ldap{nullptr};
	bool _bindSuccessful{false};
	ULONG _rc{0};

	Poco::Timespan _timeout{DEFAULT_TIMEOUT_SECONDS, 0};
	Poco::Timespan _bindTimeout;
		/// The limit for connecting and for the result of a bind; zero
		/// until setTimeout() is called, and the bind then waits without a limit.

	std::string _roleAttributeName;
	std::string _permissionAttributeName;
	std::string _rolePermissionAttributeName;
	AttributesNameMap _extraAttributesMap;
};

const std::string LDAPClientImpl::DISPLAY_NAME_ATTR{"displayName"s};
const std::string LDAPClientImpl::ACCOUNT_NAME_ATTR{"sAMAccountName"s};
const std::string LDAPClientImpl::USER_PRINCIPAL_NAME_ATTR{"userPrincipalName"s};
const std::string LDAPClientImpl::PRINCIPAL_NAME_ATTR{"msDS-PrincipalName"s};


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

		if (_ldap == nullptr)
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
	if (_ldap == nullptr) return false;

	_rc = ldap_set_optionW(_ldap, LDAP_OPT_TIMELIMIT, &limit);
	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Failed to set time limit: %s"s, ldapError(_rc));
		return false;
	}
	return true;
}


bool LDAPClientImpl::setTimeout(const Poco::Timespan& timeout)
{
	if (_ldap == nullptr)
	{
		return false;
	}

	_timeout = timeout;
	_bindTimeout = timeout;
	l_timeval tv = toTimeval(timeout);
	_rc = ldap_set_optionW(_ldap, LDAP_OPT_SEND_TIMEOUT, &tv);
	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Failed to set LDAP send timeout: %s"s, ldapError(_rc));
		return false;
	}
	return true;
}


LDAPClientImpl::Result LDAPClientImpl::bind(const std::string& bindDN, const std::string& credentials)
{
	if (_ldap == nullptr)
	{
		_logger.error("LDAP not initialized");
		return Result::ERROR_RESULT;
	}

	_bindSuccessful = false;
	if (!LDAPClient::isValidBindValue(credentials))
	{
		_logger.debug("LDAP bind refused without contacting the server (password empty or containing a NUL character): "
			"%s."s, bindDN);
		return Result::WRONG_CREDENTIALS;
	}

	// Convert bindDN and credentials to wide strings
	std::wstring wbindDN;
	std::wstring wcreds;
	Poco::UnicodeConverter::toUTF16(bindDN, wbindDN);
	Poco::UnicodeConverter::toUTF16(credentials, wcreds);

	if (_bindTimeout > 0)
	{
		// ldap_simple_bind_sW() would wait for the connection and for the result without a limit.
		l_timeval timeout = toTimeval(_bindTimeout);
		_rc = ldap_connect(_ldap, &timeout);
		if (_rc == LDAP_SUCCESS)
		{
			const ULONG msgId = ldap_simple_bindW(_ldap, const_cast<LPWSTR>(wbindDN.c_str()),
				const_cast<LPWSTR>(wcreds.c_str()));
			if (msgId == static_cast<ULONG>(-1))
			{
				_rc = LdapGetLastError();
			}
			else
			{
				LDAPMessage* pMessage = nullptr;
				const ULONG resultType = ldap_result(_ldap, msgId, LDAP_MSG_ALL, &timeout, &pMessage);
				if (resultType == 0)
				{
					ldap_abandon(_ldap, msgId);
					_rc = LDAP_TIMEOUT;
				}
				else if (resultType == static_cast<ULONG>(-1))
				{
					_rc = LdapGetLastError();
				}
				else
				{
					_rc = ldap_result2error(_ldap, pMessage, TRUE);
				}
			}
		}
	}
	else
	{
		_rc = ldap_simple_bind_sW(_ldap, const_cast<LPWSTR>(wbindDN.c_str()), const_cast<LPWSTR>(wcreds.c_str()));
	}
	Poco::secureClear(wcreds);

	const Result result = LDAPClient::bindResult(static_cast<int>(_rc));
	if (result == Result::SUCCESS_RESULT)
	{
		_logger.debug("LDAP bind successful: %s."s, bindDN);
		_bindSuccessful = true;
	}
	else if (_rc == LDAP_INVALID_CREDENTIALS)
	{
		_logger.debug("LDAP bind failed (invalid credentials): %s."s, bindDN);
	}
	else if (result == Result::SERVER_INACCESSIBLE)
	{
		_logger.error("LDAP server inaccessible for %s: %s"s, bindDN, ldapError(_rc));
	}
	else
	{
		_logger.error("LDAP bind failed for %s: %s"s, bindDN, ldapError(_rc));
	}
	return result;
}


bool LDAPClientImpl::isBound() const
{
	return _ldap != nullptr && _bindSuccessful;
}


void LDAPClientImpl::unbind()
{
	if (_ldap != nullptr)
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


bool LDAPClientImpl::find(const std::string& searchBase, const std::string& filter, bool subtree, bool queryRoles,
	bool queryAccountName, AuthProperties& properties)
{
	if (_ldap == nullptr)
	{
		_logger.error("LDAP not initialized");
		return false;
	}

	// display name, account name, user principal name, principal name, permission and role attribute
	constexpr std::size_t MAX_FIXED_ATTRS = 6;
	constexpr std::size_t MAX_LDAP_ATTRS = MAX_FIXED_ATTRS + LDAPClient::MAX_EXTRA_ATTRIBUTES;
	LPWSTR pSearchAttr[MAX_LDAP_ATTRS + 1];
	std::size_t nSearchAttr = 0;

	const auto& permAttrName{ queryRoles ? _rolePermissionAttributeName : _permissionAttributeName };

	// Convert attribute names to wide strings on the fly and store pointers.
	// We'll allocate temporary wstrings to hold them and keep pointers valid for the call.
	std::vector<std::wstring> tmpAttrs;
	// pSearchAttr points into the strings, so the vector must not reallocate.
	tmpAttrs.reserve(MAX_LDAP_ATTRS);
	auto pushAttr = [&](const std::string& a)
	{
		tmpAttrs.emplace_back();
		Poco::UnicodeConverter::toUTF16(a, tmpAttrs.back());
		pSearchAttr[nSearchAttr++] = const_cast<LPWSTR>(tmpAttrs.back().c_str());
	};

	// displayName
	pushAttr(DISPLAY_NAME_ATTR);
	if (queryAccountName)
	{
		pushAttr(ACCOUNT_NAME_ATTR);
		pushAttr(USER_PRINCIPAL_NAME_ATTR);
		pushAttr(PRINCIPAL_NAME_ATTR);
	}
	// permission or role-permission attribute
	if (!permAttrName.empty()) pushAttr(permAttrName);
	// role attribute
	if (!_roleAttributeName.empty()) pushAttr(_roleAttributeName);
	// extra attributes
	const std::size_t extraCount = std::min(_extraAttributesMap.size(), LDAPClient::MAX_EXTRA_ATTRIBUTES);
	for (std::size_t i = 0; i < extraCount; ++i)
	{
		pushAttr(_extraAttributesMap[i].first);
	}
	pSearchAttr[nSearchAttr] = nullptr;

	LDAPMessage* pRawMessage = nullptr;

	// Build wide variants of searchBase and filter
	std::wstring wSearchBase;
	std::wstring wFilter;
	Poco::UnicodeConverter::toUTF16(searchBase, wSearchBase);
	Poco::UnicodeConverter::toUTF16(filter, wFilter);

	// Setup timeout (l_timeval used by WinLDAP)
	l_timeval timeout = toTimeval(_timeout);

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
		&pRawMessage
	);
	const std::unique_ptr<LDAPMessage, MessageDeleter> pMessage(pRawMessage);

	_logger.debug("Search \"%s\" \"%s\" attribute count %z: rc = %d"s, searchBase, filter, nSearchAttr, _rc);
	for (std::size_t i{0}; i < nSearchAttr; i++)
	{
		// convert attr back to UTF-8 for logging
		std::string attrUtf8;
		Poco::UnicodeConverter::toUTF8(tmpAttrs[i], attrUtf8);
		_logger.debug("LDAP query attr: %s"s, attrUtf8);
	}

	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("LDAP search failed: %s"s, ldapError(_rc));
		return false;
	}

	std::string displayName;
	std::string distinguishedName;
	Permissions ldapPermissions;
	Roles ldapRoles;
	Attributes ldapAttributes;
	std::vector<std::string> accountNames;
	std::vector<std::string> userPrincipalNames;
	std::vector<std::string> principalNames;
	int entryCount = 0;

	for (LDAPMessage* pEntry = ldap_first_entry(_ldap, pMessage.get()); pEntry != nullptr;
		pEntry = ldap_next_entry(_ldap, pEntry))
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
		BerElement* pRawBer = nullptr;
		LPWSTR pAttr = ldap_first_attributeW(_ldap, pEntry, &pRawBer);
		const std::unique_ptr<BerElement, BerElementDeleter> pBer(pRawBer);

		for (; pAttr != nullptr; pAttr = ldap_next_attributeW(_ldap, pEntry, pBer.get()))
		{
			// The values are read with their length as UTF-8 bytes, so that a NUL character does not shorten them.
			berval** ppValues = ldap_get_values_lenW(_ldap, pEntry, pAttr);
			if (ppValues != nullptr)
			{
				for (int i = 0; ppValues[i] != nullptr; ++i)
				{
					std::string value(ppValues[i]->bv_val, ppValues[i]->bv_len);
					Poco::trimInPlace(value);

					// convert attribute name to UTF-8 for comparisons/lookup
					std::wstring wattr(pAttr);
					std::string attr;
					Poco::UnicodeConverter::toUTF8(wattr, attr);

					std::string canonicalAttr;

					// Not one of the alternatives below, as an extra attribute may map it too.
					if (queryAccountName && Poco::UTF8::icompare(ACCOUNT_NAME_ATTR, attr) == 0)
					{
						accountNames.push_back(value);
					}
					else if (queryAccountName && Poco::UTF8::icompare(USER_PRINCIPAL_NAME_ATTR, attr) == 0)
					{
						userPrincipalNames.push_back(value);
					}
					else if (queryAccountName && Poco::UTF8::icompare(PRINCIPAL_NAME_ATTR, attr) == 0)
					{
						principalNames.push_back(value);
					}
					if (Poco::UTF8::icompare(DISPLAY_NAME_ATTR, attr) == 0)
					{
						if (_logger.debug())
						{
							_logger.debug("Found display name: \"%s\""s, value);
						}
						displayName = value;
					}
					else if (Poco::UTF8::icompare(_permissionAttributeName, attr) == 0 ||
						Poco::UTF8::icompare(_rolePermissionAttributeName, attr) == 0)
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
				ldap_value_free_len(ppValues);
			}

			// Free attribute string (wide)
			ldap_memfreeW(pAttr);
		}
	}

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
	properties.accountNames = std::move(accountNames);
	properties.userPrincipalNames = std::move(userPrincipalNames);
	properties.principalNames = std::move(principalNames);

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

} // namespace Poco::OSP::Auth
