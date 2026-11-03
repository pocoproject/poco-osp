//
// LDAPClient_POSIX.cpp
//
// Library: OSP
// Package: Auth
// Module:  LDAPClient
//
// Implementation of the LDAPClientImpl members that use OpenLDAP.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LDAPClientImpl.h"
#include "Poco/Logger.h"
#include <algorithm>
#include <memory>
#include <string>
#include <ldap.h>
#include <sys/time.h>


namespace {
	[[nodiscard]] std::string ldapError(int err)
	{
  		return ldap_err2string(err);
	}


	[[nodiscard]] ::timeval toTimeval(const Poco::Timespan& timespan)
	{
		return ::timeval{static_cast<time_t>(timespan.totalSeconds()), static_cast<suseconds_t>(timespan.useconds())};
	}


	struct MessageDeleter
	{
		void operator()(::LDAPMessage* pMessage) const
		{
			::ldap_msgfree(pMessage);
		}
	};


	struct BerElementDeleter
	{
		void operator()(::BerElement* pBerElement) const
		{
			::ber_free(pBerElement, 0);
		}
	};
}


namespace Poco::OSP::Auth {


bool LDAPClientImpl::initialize(const std::string& uri)
{
	_rc = ::ldap_initialize(&_ldap, uri.c_str());

	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Cannot initialize the LDAP connection to %s: %s"s, uri, ldapError(_rc));
		return false;
	}

	int pv = 3;
	_rc = ::ldap_set_option(_ldap, LDAP_OPT_PROTOCOL_VERSION, &pv);
	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Cannot set LDAP version 3 for the connection to %s: %s"s, uri, ldapError(_rc));
		return false;
	}

	_logger.debug("LDAP initialize successful: %s."s, uri);

	return true;
}


bool LDAPClientImpl::setTimeLimit(int limit)
{
	_rc = ::ldap_set_option(_ldap, LDAP_OPT_TIMELIMIT, &limit);
	return (_rc == LDAP_SUCCESS);
}


bool LDAPClientImpl::setTimeout(const Poco::Timespan& timeout)
{
	if (_ldap == nullptr)
	{
		return false;
	}

	_timeout = timeout;
	const ::timeval tv = toTimeval(timeout);
	_rc = ::ldap_set_option(_ldap, LDAP_OPT_NETWORK_TIMEOUT, &tv);
	if (_rc == LDAP_SUCCESS)
	{
		_rc = ::ldap_set_option(_ldap, LDAP_OPT_TIMEOUT, &tv);
	}
	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Failed to set LDAP timeout: %s", ldapError(_rc));
		return false;
	}
	return true;
}


LDAPClientImpl::Result LDAPClientImpl::bind(const std::string& bindDN, const std::string& credentials)
{
	_bindSuccessful = false;
	if (!LDAPClient::isValidBindValue(credentials))
	{
		_logger.debug("LDAP bind refused without contacting the server (password empty or containing a NUL character): "
			"%s."s, bindDN);
		return Result::WRONG_CREDENTIALS;
	}

	::berval password{};
	password.bv_val = const_cast<char*>(credentials.data());
	password.bv_len = credentials.size();
	_rc = ::ldap_sasl_bind_s(_ldap, bindDN.c_str(), LDAP_SASL_SIMPLE, &password, nullptr, nullptr, nullptr);
	const Result result = LDAPClient::bindResult(_rc);
	if (result == Result::SUCCESS_RESULT)
	{
		_logger.debug("LDAP bind successful: %s."s, bindDN);
		_bindSuccessful = true;
	}
	else if (_rc == LDAP_INVALID_CREDENTIALS)
	{
		_logger.debug("LDAP bind failed (invalid credentials): %s."s, bindDN);
	}
	else
	{
		_logger.error("LDAP bind failed for %s: %s"s, bindDN, std::string(ldapError(_rc)));
	}
	return result;
}


bool LDAPClientImpl::isBound() const
{
	return (_ldap != nullptr && _bindSuccessful);
}


void LDAPClientImpl::unbind()
{
	if (_ldap == nullptr)
	{
		return;
	}

	int rc = ::ldap_unbind_ext_s(_ldap, nullptr, nullptr);
	if (rc != LDAP_SUCCESS)
	{
		_logger.warning("LDAP unbind error (ignored): %s", ldapError(rc));
	}
	_ldap = nullptr;
	_bindSuccessful = false;
	_rc = 0;
}


bool LDAPClientImpl::find(const std::string& searchBase, const std::string& filter, bool subtree,
	bool rolePermissions, bool entryRequired, bool queryAccountName, AuthProperties& properties)
{
	// display name, account name, user principal name, principal name, permission and role attribute
	constexpr std::size_t MAX_FIXED_ATTRS = 6;
	constexpr std::size_t MAX_LDAP_ATTRS = MAX_FIXED_ATTRS + LDAPClient::MAX_EXTRA_ATTRIBUTES;
	char* pSearchAttr[MAX_LDAP_ATTRS + 1];
	std::size_t nSearchAttr = 0;

	const auto& permAttrName { rolePermissions ? _rolePermissionAttributeName : _permissionAttributeName};

	pSearchAttr[nSearchAttr++] = const_cast<char*>(DISPLAY_NAME_ATTR.c_str());
	if (queryAccountName)
	{
		pSearchAttr[nSearchAttr++] = const_cast<char*>(ACCOUNT_NAME_ATTR.c_str());
		pSearchAttr[nSearchAttr++] = const_cast<char*>(USER_PRINCIPAL_NAME_ATTR.c_str());
		pSearchAttr[nSearchAttr++] = const_cast<char*>(PRINCIPAL_NAME_ATTR.c_str());
	}
	if (!permAttrName.empty())
	{
		pSearchAttr[nSearchAttr++] = const_cast<char*>(permAttrName.c_str());
	}
	if (!_roleAttributeName.empty())
	{
		pSearchAttr[nSearchAttr++] = const_cast<char*>(_roleAttributeName.c_str());
	}
	const std::size_t extraCount = std::min(_extraAttributesMap.size(), LDAPClient::MAX_EXTRA_ATTRIBUTES);
	for (std::size_t i = 0; i < extraCount; ++i)
	{
		pSearchAttr[nSearchAttr++] = const_cast<char*>(_extraAttributesMap[i].first.c_str());
	}
	pSearchAttr[nSearchAttr] = nullptr;

	::LDAPMessage* pRawMessage {nullptr};
	::timeval timeout = toTimeval(_timeout);
	_rc = ::ldap_search_ext_s(
		_ldap,
		const_cast<char*>(searchBase.c_str()),
		subtree ? LDAP_SCOPE_SUBTREE : LDAP_SCOPE_BASE,
		const_cast<char*>(filter.c_str()),
		pSearchAttr,
		0,
		nullptr,
		nullptr,
		&timeout,
		1000,	 // size limit
		&pRawMessage
	);
	// The result must be freed also when the search fails.
	const std::unique_ptr<::LDAPMessage, MessageDeleter> pMessage(pRawMessage);

	if (_logger.debug())
	{
		_logger.debug("Search \"%s\" \"%s\" attribute count %z: = %d"s, searchBase, filter, nSearchAttr, _rc);
		for (std::size_t i {0}; i < nSearchAttr; i++)
		{
			_logger.debug("LDAP query attr: %s"s, std::string(pSearchAttr[i]));
		}
	}
	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("The LDAP search in %s with the filter %s failed: %s"s, searchBase, filter, ldapError(_rc));
		return false;
	}
	AuthProperties found;
	int entryCount {0};
	for (auto* pEntry = ::ldap_first_entry(_ldap, pMessage.get()); pEntry != nullptr;
		pEntry = ::ldap_next_entry(_ldap, pEntry))
	{
		entryCount++;
		char* pDN = ::ldap_get_dn(_ldap, pEntry);
		if (pDN != nullptr)
		{
			found.distinguishedName = pDN;
			::ldap_memfree(pDN);
		}
		::BerElement* pRawBerElement{nullptr};
		char* pAttr = ::ldap_first_attribute(_ldap, pEntry, &pRawBerElement);
		const std::unique_ptr<::BerElement, BerElementDeleter> pBerElement(pRawBerElement);
		for (; pAttr != nullptr; pAttr = ::ldap_next_attribute(_ldap, pEntry, pBerElement.get()))
		{
			::berval** ppValues = ::ldap_get_values_len(_ldap, pEntry, pAttr);
			if (ppValues != nullptr)
			{
				const std::string attr(pAttr);
				for (int i = 0; ppValues[i] != nullptr; i++)
				{
					addValue(attr, std::string(ppValues[i]->bv_val, ppValues[i]->bv_len), queryAccountName, found);
				}
				::ldap_value_free_len(ppValues);
			}
			::ldap_memfree(pAttr);
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
