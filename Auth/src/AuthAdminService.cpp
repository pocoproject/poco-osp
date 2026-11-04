//
// AuthAdminService.cpp
//
// Library: OSP/Auth
// Package: Auth
// Module:  AuthAdminService
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/String.h"
#include <string>


namespace Poco::OSP::Auth {


const std::string AuthAdminService::SERVICE_NAME("osp.auth"s);
const std::string AuthAdminService::USER_ATTR_FULL_NAME("auth_full_name"s);
const std::string AuthAdminService::USER_ATTR_LDAP("auth_ldap"s);
const std::string AuthAdminService::USER_ATTR_SUCCESS_TIME("auth_success_time_t");
const std::string AuthAdminService::ROLE_ATTR_LDAP("auth_ldap_role"s);
const std::string AuthAdminService::AUTH_ATTRIBUTE_PREFIX("auth_"s);


const std::type_info& AuthAdminService::type() const
{
	return typeid(AuthAdminService);
}


bool AuthAdminService::isA(const std::type_info& otherType) const
{
	return otherType == typeid(AuthAdminService) || AuthService::isA(otherType);
}


bool AuthAdminService::isAuthAttribute(const std::string& attribute)
{
	return Poco::icompare(attribute, 0, AUTH_ATTRIBUTE_PREFIX.size(), AUTH_ATTRIBUTE_PREFIX) == 0;
}


} // namespace Poco::OSP::Auth
