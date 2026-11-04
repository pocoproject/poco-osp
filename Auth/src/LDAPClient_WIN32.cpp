//
// LDAPClient_WIN32.cpp
//
// Library: OSP
// Package: Auth
// Module:  LDAPClient
//
// Implementation of the LDAPClientImpl members that use WinLDAP.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "LDAPClientImpl.h"
#include "Poco/Logger.h"
#include "Poco/String.h"
#include "Poco/URI.h"
#include "Poco/UnicodeConverter.h"
#include <algorithm>
#include <memory>
#include <string>
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


bool LDAPClientImpl::find(const std::string& searchBase, const std::string& filter, bool subtree,
	bool rolePermissions, bool entryRequired, bool queryAccountName, AuthProperties& properties)
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

	const auto& permAttrName{ rolePermissions ? _rolePermissionAttributeName : _permissionAttributeName };

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

	if (_logger.debug())
	{
		_logger.debug("Search \"%s\" \"%s\" attribute count %z: rc = %d"s, searchBase, filter, nSearchAttr, _rc);
		for (std::size_t i{0}; i < nSearchAttr; i++)
		{
			// convert attr back to UTF-8 for logging
			std::string attrUtf8;
			Poco::UnicodeConverter::toUTF8(tmpAttrs[i], attrUtf8);
			_logger.debug("LDAP query attr: %s"s, attrUtf8);
		}
	}

	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("The LDAP search in %s with the filter %s failed: %s"s, searchBase, filter, ldapError(_rc));
		return false;
	}

	AuthProperties found;
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
				Poco::UnicodeConverter::toUTF8(wdn, found.distinguishedName);
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
				std::string attr;
				Poco::UnicodeConverter::toUTF8(std::wstring(pAttr), attr);
				for (int i = 0; ppValues[i] != nullptr; ++i)
				{
					addValue(attr, std::string(ppValues[i]->bv_val, ppValues[i]->bv_len), queryAccountName, found);
				}
				ldap_value_free_len(ppValues);
			}

			// Free attribute string (wide)
			ldap_memfreeW(pAttr);
		}
	}

	if (!checkEntryCount(entryCount, entryRequired, searchBase, filter))
	{
		return false;
	}
	properties = std::move(found);
	return true;
}


} // namespace Poco::OSP::Auth
