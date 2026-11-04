//
// AuthNotification.cpp
//
// Library: OSP/Auth
// Package: Auth
// Module:  AuthNotification
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/AuthNotification.h"


namespace Poco::OSP::Auth {


AuthNotification::AuthNotification(bool isExternal,
	const std::string& userName,
	const std::string& fullName,
	const Roles& roles,
	const Permissions& permissions,
	const Attributes& attributes) :_isExternal(isExternal),
		_userName(userName), _fullName(fullName), _roles(roles),
		_permissions(permissions), _attributes(attributes)
{
}


AuthNotification::~AuthNotification() = default;


} // namespace Poco::OSP::Auth
