//
// PasswdCommandTest.h
//
// Definition of the PasswdCommandTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef PasswdCommandTest_INCLUDED
#define PasswdCommandTest_INCLUDED


#include "TestAuthAdminService.h"
#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class TemporaryBundle;


class PasswdCommandTest: public CppUnit::TestCase
	/// Tests the passwd command.
{
public:
	PasswdCommandTest() = delete;
	explicit PasswdCommandTest(const std::string& name);
	~PasswdCommandTest() override;

	PasswdCommandTest(const PasswdCommandTest&) = delete;
	PasswdCommandTest(PasswdCommandTest&&) = delete;
	PasswdCommandTest& operator = (const PasswdCommandTest&) = delete;
	PasswdCommandTest& operator = (PasswdCommandTest&&) = delete;

	void setUp() override;
	void tearDown() override;

	void testPasswdWithoutLoginIsDenied();
	void testPasswdOfOtherUserNeedsAdminPermission();
	void testUserChangesOwnPassword();
	void testAdministratorChangesPasswords();
	void testHelpHasNoPrompt();
	void testHelpNamesAccountName();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<TemporaryBundle> _pBundle;
	TestAuthAdminService::Ptr _pAuthService;
};


#endif // PasswdCommandTest_INCLUDED
