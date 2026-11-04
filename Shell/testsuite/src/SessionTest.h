//
// SessionTest.h
//
// Definition of the SessionTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SessionTest_INCLUDED
#define SessionTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class ShellFixture;


class SessionTest: public CppUnit::TestCase
{
public:
	SessionTest() = delete;
	explicit SessionTest(const std::string& name);
	~SessionTest() override;

	void setUp() override;
	void tearDown() override;

	void testLoginWithValidCredentials();
	void testLoginKeepsAccountName();
	void testLoginWithWrongPassword();
	void testRejectedLoginLeavesNoUser();
	void testAuthorizeChecksPermission();
	void testAuthorizeBeforeLogin();
	void testEmptyServiceNameDisablesAuthentication();
	void testLoginWithKey();
	void testMissingAuthServiceDeniesLogin();
	void testMissingAuthServiceDeniesPermissions();
	void testUnregisteredAuthServiceIsNotUsed();
	void testAuthServiceExceptionRejectsLogin();
	void testStdExceptionFromAuthServiceRejectsLogin();
	void testServiceOfOtherTypeRejectsLogin();
	void testAuthServiceExceptionDeniesPermission();
	void testServiceOfOtherTypeDeniesPermission();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // SessionTest_INCLUDED
