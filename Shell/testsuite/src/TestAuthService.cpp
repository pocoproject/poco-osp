//
// TestAuthService.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestAuthService.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/Exception.h"
#include <stdexcept>
#include <string>


const std::string TestAuthService::ADMIN("admin");
const std::string TestAuthService::ADMIN_PASSWORD("secret");
const std::string TestAuthService::ADMIN_OTHER_NAME("Admin@example.com");
const std::string TestAuthService::GUEST("guest");
const std::string TestAuthService::GUEST_PASSWORD("guest");
const std::string TestAuthService::FAILING_USER("failing");
const std::string TestAuthService::STD_FAILING_USER("failingStd");
const std::string TestAuthService::ADMIN_PERMISSION("bundleAdmin");
const std::string TestAuthService::LOG_PERMISSION("logAdmin");
const std::string TestAuthService::FAILING_PERMISSION("failingPermission");
const std::string TestAuthService::STD_FAILING_PERMISSION("failingStdPermission");


bool TestAuthService::authenticate(const std::string& userName, const std::string& credentials) const
{
	if (userName == FAILING_USER) throw Poco::IOException("The user database is not available");
	if (userName == STD_FAILING_USER) throw std::runtime_error("The user database is not available");

	return (userName == ADMIN && credentials == ADMIN_PASSWORD) ||
		(userName == GUEST && credentials == GUEST_PASSWORD);
}


bool TestAuthService::authenticateAccount(const std::string& userName, const std::string& credentials,
	std::string& accountName) const
{
	if (userName == ADMIN_OTHER_NAME && credentials == ADMIN_PASSWORD)
	{
		accountName = ADMIN;
		return true;
	}
	return AuthService::authenticateAccount(userName, credentials, accountName);
}


bool TestAuthService::authorize(const std::string& userName, const std::string& permission) const
{
	if (permission == FAILING_PERMISSION) throw Poco::IOException("The permission database is not available");
	if (permission == STD_FAILING_PERMISSION) throw std::runtime_error("The permission database is not available");

	return userName == ADMIN && (permission == ADMIN_PERMISSION || permission == LOG_PERMISSION);
}


bool TestAuthService::authorize(const std::string& userName, const std::string& /*roleOrScope*/,
	const std::string& permission) const
{
	return authorize(userName, permission);
}


bool TestAuthService::userExists(const std::string& userName) const
{
	return userName == ADMIN || userName == GUEST;
}
