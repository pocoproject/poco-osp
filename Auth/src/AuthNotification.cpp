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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/AuthNotification.h"


namespace Poco {
namespace OSP {
namespace Auth {


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


AuthNotification::~AuthNotification()
{

}


} } } // namespace Poco::OSP::Auth
