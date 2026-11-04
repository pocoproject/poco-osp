//
// TestAuthService.h
//
// Definition of the TestAuthService class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestAuthService_INCLUDED
#define TestAuthService_INCLUDED


#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/AutoPtr.h"
#include <string>


class TestAuthService: public Poco::OSP::Auth::AuthService
	/// Knows the user "admin" with the password "secret" and the permissions
	/// "bundleAdmin" and "logAdmin", and the user "guest" with the password
	/// "guest" and no permission. authenticate() throws a Poco::IOException
	/// for the user "failing" and a std::runtime_error for the user "failingStd".
	/// authorize() throws a Poco::IOException for the permission
	/// "failingPermission" and a std::runtime_error for the permission
	/// "failingStdPermission". authenticateAccount() also accepts "admin"
	/// under the name "Admin@example.com", which authenticate() refuses, and
	/// returns the account name "admin" for it. The users "dash", "--" and
	/// "quote" have no permission; their user names or passwords start with
	/// '-' or contain quotes and backslashes.
{
public:
	using Ptr = Poco::AutoPtr<TestAuthService>;

	TestAuthService() = default;
		/// Creates the TestAuthService.

	TestAuthService(const TestAuthService&) = delete;
	TestAuthService(TestAuthService&&) = delete;
	TestAuthService& operator = (const TestAuthService&) = delete;
	TestAuthService& operator = (TestAuthService&&) = delete;

	// AuthService
	[[nodiscard]] bool authenticate(const std::string& userName, const std::string& credentials) const override;
	[[nodiscard]] bool authenticateAccount(const std::string& userName, const std::string& credentials,
		std::string& accountName) const override;
	[[nodiscard]] bool authorize(const std::string& userName, const std::string& permission) const override;
	[[nodiscard]] bool authorize(const std::string& userName, const std::string& roleOrScope,
		const std::string& permission) const override;
	[[nodiscard]] bool userExists(const std::string& userName) const override;

	static const std::string ADMIN;
	static const std::string ADMIN_PASSWORD;
	static const std::string ADMIN_OTHER_NAME;
	static const std::string GUEST;
	static const std::string GUEST_PASSWORD;
	static const std::string DASH_USER;
	static const std::string DASH_PASSWORD;
	static const std::string DOUBLE_DASH_USER;
	static const std::string DOUBLE_DASH_PASSWORD;
	static const std::string QUOTE_USER;
	static const std::string QUOTE_PASSWORD;
	static const std::string FAILING_USER;
	static const std::string STD_FAILING_USER;
	static const std::string ADMIN_PERMISSION;
	static const std::string LOG_PERMISSION;
	static const std::string FAILING_PERMISSION;
	static const std::string STD_FAILING_PERMISSION;

protected:
	~TestAuthService() override = default;
		/// Reference counted: destroyed through release() only.
};


#endif // TestAuthService_INCLUDED
