//
// LDAPClient_POSIX.cpp
//
// Library: OSP
// Package: Auth
// Module:  LDAPClient
//
// Definition of the LDAPClientImpl POSIX class using OpenLDAP.
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
#include "Poco/OSP/Auth/LDAPClient.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <ldap.h>
#include <sys/time.h>

namespace {
	std::string ldapError(int err)
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
		std::vector<std::string> accountNames;
			/// The values of sAMAccountName, if it is queried.
		std::vector<std::string> userPrincipalNames;
			/// The values of userPrincipalName, queried with sAMAccountName.
		std::vector<std::string> principalNames;
			/// The values of msDS-PrincipalName, queried with sAMAccountName.
	};

	LDAPClientImpl(Poco::Logger& logger);
		/// Posix LDAPClient implementation.

	~LDAPClientImpl();

	bool initialize(const std::string& url);

	bool setTimeLimit(int limit);

	Result bind(const std::string& bindDN, const std::string& credentials);

	bool isBound() const;

	void unbind();

	[[nodiscard]] bool setTimeout(const Poco::Timespan& timeout);
		/// Sets the timeout for connecting and for the result of each operation.

	void setRoleAttributeName(const std::string& name);
	void setPermissionAttributeName(const std::string& name);
	void setRolePermissionAttributeName(const std::string& name);
	void setExtraAttributesMap(const AttributesNameMap& nameMap);

	[[nodiscard]] bool find(const std::string& searchBase, const std::string& filter, bool subtree, bool queryRoles,
		bool queryAccountName, AuthProperties& properties);

	int lastReturnCode() const;
		/// Return code of the last LDAP operation.

private:

	bool isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const;

	static const std::string DISPLAY_NAME_ATTR;
	static const std::string ACCOUNT_NAME_ATTR;
	static const std::string USER_PRINCIPAL_NAME_ATTR;
	static const std::string PRINCIPAL_NAME_ATTR;
	static constexpr long DEFAULT_TIMEOUT_SECONDS = 30;

	Poco::Logger& _logger;
	::LDAP* _ldap{nullptr};
	bool _bindSuccessful{false};
	int _rc{0};

	Poco::Timespan _timeout {DEFAULT_TIMEOUT_SECONDS, 0};

	std::string _roleAttributeName;
	std::string _permissionAttributeName;
	std::string _rolePermissionAttributeName;
	AttributesNameMap _extraAttributesMap;
};

const std::string LDAPClientImpl::DISPLAY_NAME_ATTR {"displayName"s};
const std::string LDAPClientImpl::ACCOUNT_NAME_ATTR {"sAMAccountName"s};
const std::string LDAPClientImpl::USER_PRINCIPAL_NAME_ATTR {"userPrincipalName"s};
const std::string LDAPClientImpl::PRINCIPAL_NAME_ATTR {"msDS-PrincipalName"s};


LDAPClientImpl::LDAPClientImpl(Poco::Logger& logger) : _logger{logger}
{
}


LDAPClientImpl::~LDAPClientImpl()
{
	(void)unbind();
}


bool LDAPClientImpl::initialize(const std::string& uri)
{
	_rc = ::ldap_initialize(&_ldap, uri.c_str());

	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Failed to initialize LDAP: %s", ldapError(_rc));
		return false;
	}

	int pv = 3;
	_rc = ::ldap_set_option(_ldap, LDAP_OPT_PROTOCOL_VERSION, &pv);
	if (_rc != LDAP_SUCCESS)
	{
		_logger.error("Failed to set LDAP version 3 option: %s", ldapError(_rc));
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
	if (!_ldap)
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
	// display name, account name, user principal name, principal name, permission and role attribute
	constexpr std::size_t MAX_FIXED_ATTRS = 6;
	constexpr std::size_t MAX_LDAP_ATTRS = MAX_FIXED_ATTRS + LDAPClient::MAX_EXTRA_ATTRIBUTES;
	char* pSearchAttr[MAX_LDAP_ATTRS + 1];
	std::size_t nSearchAttr = 0;

	const auto& permAttrName { queryRoles ? _rolePermissionAttributeName : _permissionAttributeName};

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

	_logger.debug("Search \"%s\" \"%s\" attribute count %z: = %d"s, searchBase, filter, nSearchAttr, _rc);
	for (std::size_t i {0}; i < nSearchAttr; i++)
	{
		_logger.debug("LDAP query attr: %s"s, std::string(pSearchAttr[i]));
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
	int entryCount {0};
	for (auto* pEntry = ::ldap_first_entry(_ldap, pMessage.get()); pEntry != nullptr;
		pEntry = ::ldap_next_entry(_ldap, pEntry))
	{
		entryCount++;
		char* pDN = ::ldap_get_dn(_ldap, pEntry);
		if (pDN != nullptr)
		{
			distinguishedName = pDN;
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
				for (int i = 0; ppValues[i] != nullptr; i++)
				{
					std::string value(ppValues[i]->bv_val, ppValues[i]->bv_len);
					Poco::trimInPlace(value);
					std::string attr(pAttr);
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
				::ldap_value_free_len(ppValues);
			}
			::ldap_memfree(pAttr);
		}
	}

	if (entryCount < 1)
	{
		if (!queryRoles)
		{
			// When querying for user's own LDAP record, it is an error if it can't be found.
			_logger.warning("LDAP: record not found for: \"%s\", \"%s\""s, searchBase, filter);
			return false;
		}
		// It is possible that the LDAP record is not found when trying to find a
		// LDAP group. This is not treated as an error.
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
	return _rc;
}


bool LDAPClientImpl::isExtraAttribute(const std::string& attr, std::string& canonicalAttr) const
{
	for (const auto& el: _extraAttributesMap)
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
