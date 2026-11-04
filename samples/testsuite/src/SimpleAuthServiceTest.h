//
// SimpleAuthServiceTest.h
//
// Definition of the SimpleAuthServiceTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SimpleAuthServiceTest_INCLUDED
#define SimpleAuthServiceTest_INCLUDED


#include "CppUnit/TestCase.h"


class SimpleAuthServiceTest: public CppUnit::TestCase
{
public:
	SimpleAuthServiceTest() = delete;
	explicit SimpleAuthServiceTest(const std::string& name);
	~SimpleAuthServiceTest() override;

	void testServiceIsNotRegisteredWithoutAdminPassword();
	void testUserCannotLogInWithoutPassword();
	void testPasswordsAreChecked();
	void testUserHasNoPermissionsByDefault();
	void testRestartWithoutAdminPasswordKeepsOtherAuthService();

	static CppUnit::Test* suite();
};


#endif // SimpleAuthServiceTest_INCLUDED
