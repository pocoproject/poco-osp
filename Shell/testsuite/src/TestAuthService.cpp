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
#include <map>
#include <stdexcept>
#include <string>


const std::string TestAuthService::ADMIN("admin");
const std::string TestAuthService::ADMIN_PASSWORD("secret");
const std::string TestAuthService::ADMIN_OTHER_NAME("Admin@example.com");
const std::string TestAuthService::GUEST("guest");
const std::string TestAuthService::GUEST_PASSWORD("guest");
const std::string TestAuthService::DASH_USER("dash");
const std::string TestAuthService::DASH_PASSWORD("-h");
const std::string TestAuthService::DOUBLE_DASH_USER("--");
const std::string TestAuthService::DOUBLE_DASH_PASSWORD("dashes");
const std::string TestAuthService::QUOTE_USER("quote");
const std::string TestAuthService::QUOTE_PASSWORD("a\\b\"c'd\\");
const std::string TestAuthService::FAILING_USER("failing");
const std::string TestAuthService::STD_FAILING_USER("failingStd");
const std::string TestAuthService::ADMIN_PERMISSION("bundleAdmin");
const std::string TestAuthService::LOG_PERMISSION("logAdmin");
const std::string TestAuthService::FAILING_PERMISSION("failingPermission");
const std::string TestAuthService::STD_FAILING_PERMISSION("failingStdPermission");


namespace
{
	// The password of each user that authenticate() accepts. The table is made on
	// first use, after the static members it copies have been initialized.
	const std::map<std::string, std::string>& passwords()
	{
		static const std::map<std::string, std::string> table{
			{TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD},
			{TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD},
			{TestAuthService::DASH_USER, TestAuthService::DASH_PASSWORD},
			{TestAuthService::DOUBLE_DASH_USER, TestAuthService::DOUBLE_DASH_PASSWORD},
			{TestAuthService::QUOTE_USER, TestAuthService::QUOTE_PASSWORD}};
		return table;
	}
}


bool TestAuthService::authenticate(const std::string& userName, const std::string& credentials) const
{
	if (userName == FAILING_USER) throw Poco::IOException("The user database is not available");
	if (userName == STD_FAILING_USER) throw std::runtime_error("The user database is not available");

	const auto it = passwords().find(userName);
	return it != passwords().end() && it->second == credentials;
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
	return passwords().contains(userName);
}
