//
// AuthService.cpp
//
// Library: OSP
// Package: Service
// Module:  AuthService
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/ServiceFactory.h"
#include <string>


namespace Poco::OSP::Auth {


AuthService::AuthService() = default;


AuthService::~AuthService() = default;


bool AuthService::authenticateAccount(const std::string& userName, const std::string& credentials,
	std::string& accountName) const
{
	if (!authenticate(userName, credentials)) return false;

	accountName = userName;
	return true;
}


const std::type_info& AuthService::type() const
{
	return typeid(AuthService);
}

	
bool AuthService::isA(const std::type_info& otherType) const
{
	std::string name(typeid(AuthService).name());
	return name == otherType.name() || Service::isA(otherType);
}


} // namespace Poco::OSP::Auth
