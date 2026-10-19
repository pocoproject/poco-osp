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

#include <set>
#include <ldap.h>
#include <sys/time.h>

namespace {
	std::string ldapError(int err)
	{
  		return ldap_err2string(err);
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
		/// Posix LDAPClient implementation.

	~LDAPClientImpl();

	bool initialize(const std::string& url);

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
	::LDAP* _ldap{nullptr};
	bool _bindSuccessful{false};
	int _rc{0};

	int _timeoutSec {30};

	std::string _roleAttributeName;
	std::string _permissionAttributeName;
	std::string _rolePermissionAttributeName;
	AttributesNameMap _extraAttributesMap;
};

const std::string LDAPClientImpl::DISPLAY_NAME_ATTR {"displayName"s};


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


void LDAPClientImpl::setTimeout(int secs)
{
	_timeoutSec = secs;
}


LDAPClientImpl::Result LDAPClientImpl::bind(const std::string& bindDN, const std::string& credentials)
{
	Result result {Result::ERROR_RESULT};

	if (credentials.empty())
	{
		// Anonymous bind is attempted if credentials (password) is ampty.
		_logger.warning("LDAP: anonymous bind with empty credentials (%s). Some operations may fail."s, bindDN);
	}

	_bindSuccessful = false;
	_rc = ::ldap_simple_bind_s(_ldap, bindDN.c_str(), credentials.c_str());
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
		[[fallthrough]];
	case LDAP_CONNECT_ERROR:
		result = Result::SERVER_INACCESSIBLE;
		[[fallthrough]];
	default:
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

	int rc = ::ldap_unbind_s(_ldap);
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


bool LDAPClientImpl::find(const std::string& searchBase, const std::string& filter, bool subtree, bool queryRoles, AuthProperties& properties)
{
	constexpr int MAX_LDAP_ATTRS = 12;
	char* pSearchAttr[MAX_LDAP_ATTRS + 1];
	int nSearchAttr = 0;

	const auto& permAttrName { queryRoles ? _rolePermissionAttributeName : _permissionAttributeName};

	pSearchAttr[nSearchAttr++] = const_cast<char*>(DISPLAY_NAME_ATTR.c_str());
	if (!permAttrName.empty())
	{
		pSearchAttr[nSearchAttr++] = const_cast<char*>(permAttrName.c_str());
	}
	if (!_roleAttributeName.empty())
	{
		pSearchAttr[nSearchAttr++] = const_cast<char*>(_roleAttributeName.c_str());
	}
	for (auto it = _extraAttributesMap.begin(); nSearchAttr < MAX_LDAP_ATTRS && it != _extraAttributesMap.end(); ++it)
	{
		pSearchAttr[nSearchAttr++] = const_cast<char*>(it->first.c_str());
	}
	pSearchAttr[nSearchAttr] = 0;

	::LDAPMessage* pMessage {nullptr};
	::timeval timeout {_timeoutSec, 0};
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
		&pMessage
	);

	_logger.debug("Search \"%s\" \"%s\" attribute count %d: = %d"s, searchBase, filter, nSearchAttr, _rc);
	for (int i {0}; i < nSearchAttr; i++)
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
	int entryCount {0};
	for (auto* pEntry = ::ldap_first_entry(_ldap, pMessage); pEntry != nullptr; pEntry = ::ldap_next_entry(_ldap, pEntry))
	{
		entryCount++;
		char* pDN = ::ldap_get_dn(_ldap, pEntry);
		if (pDN != nullptr)
		{
			distinguishedName = pDN;
			::ldap_memfree(pDN);
		}
		::BerElement* pBerElement{nullptr};
		for (char* pAttr = ::ldap_first_attribute(_ldap, pEntry, &pBerElement); pAttr != nullptr; pAttr = ::ldap_next_attribute(_ldap, pEntry, pBerElement))
		{
			char** ppValues = ::ldap_get_values(_ldap, pEntry, pAttr);
			if (ppValues)
			{
				for (int i = 0; ppValues[i] != nullptr; i++)
				{
					std::string value(ppValues[i]);
					Poco::trimInPlace(value);
					std::string attr(pAttr);
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
				::ldap_value_free(ppValues);
			}
			::ldap_memfree(pAttr);
		}
		::ber_free(pBerElement, 0);
	}
	::ldap_msgfree(pMessage);

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
