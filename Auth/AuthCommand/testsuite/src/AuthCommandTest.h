//
// AuthCommandTest.h
//
// Definition of the AuthCommandTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef AuthCommandTest_INCLUDED
#define AuthCommandTest_INCLUDED


#include "TestAuthAdminService.h"
#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class TemporaryBundle;


class AuthCommandTest: public CppUnit::TestCase
	/// Tests the auth command.
{
public:
	AuthCommandTest() = delete;
	explicit AuthCommandTest(const std::string& name);
	~AuthCommandTest() override;

	AuthCommandTest(const AuthCommandTest&) = delete;
	AuthCommandTest(AuthCommandTest&&) = delete;
	AuthCommandTest& operator = (const AuthCommandTest&) = delete;
	AuthCommandTest& operator = (AuthCommandTest&&) = delete;

	void setUp() override;
	void tearDown() override;

	void testAuthRequiresAdminPermission();
	void testAddUserNeedsNameAndPassword();
	void testAddUserRecordsCreator();
	void testRemoveUserRefusesSessionUser();
	void testReservedAttributeIsRefusedByService();
	void testGrantAndRevokePermissions();
	void testUnlockUnlocksUser();
	void testShowUsersWithAttributeNeedsValue();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<TemporaryBundle> _pBundle;
	TestAuthAdminService::Ptr _pAuthService;
};


#endif // AuthCommandTest_INCLUDED
