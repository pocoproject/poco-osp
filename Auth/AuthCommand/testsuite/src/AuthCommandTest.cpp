//
// AuthCommandTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthCommandTest.h"
#include "AuthCommand.h"
#include "TestAuthAdminService.h"
#include "TemporaryBundle.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/DateTime.h"
#include "Poco/Exception.h"
#include "Poco/String.h"
#include <memory>
#include <set>
#include <sstream>
#include <string>


using Poco::OSP::Auth::AuthAdminService;
using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using namespace std::string_literals;


namespace
{
	const std::string NEW_USER("newuser");
	const std::string NEW_PASSWORD("newsecret");
}


AuthCommandTest::AuthCommandTest(const std::string& name): CppUnit::TestCase(name)
{
}


AuthCommandTest::~AuthCommandTest() = default;


void AuthCommandTest::setUp()
{
	_pBundle = std::make_unique<TemporaryBundle>(""s);
	_pAuthService = new TestAuthAdminService;
	_pBundle->registry().registerService(AuthAdminService::SERVICE_NAME, _pAuthService, Poco::OSP::Properties());
}


void AuthCommandTest::tearDown()
{
	_pBundle.reset();
	_pAuthService.reset();
}


void AuthCommandTest::testAuthRequiresAdminPermission()
{
	AuthCommand command(_pBundle->context());
	std::ostringstream ostr;

	Session anonymous(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		command.execute(anonymous, {"auth"s, "add"s, "user"s, NEW_USER, NEW_PASSWORD}, ostr));

	Session guest(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (guest.login(TestAuthAdminService::GUEST, TestAuthAdminService::GUEST_PASSWORD));
	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		command.execute(guest, {"auth"s, "add"s, "user"s, NEW_USER, NEW_PASSWORD}, ostr));
	assertFalse (_pAuthService->userExists(NEW_USER));
}


void AuthCommandTest::testAddUserNeedsNameAndPassword()
{
	AuthCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_USAGE),
		command.execute(admin, {"auth"s, "add"s, "user"s, NEW_USER}, ostr));
	assertEqual (static_cast<int>(Command::STATUS_USAGE),
		command.execute(admin, {"auth"s, "add"s, "user"s, NEW_USER, NEW_PASSWORD, "extra"s}, ostr));
	assertFalse (_pAuthService->userExists(NEW_USER));
}


void AuthCommandTest::testAddUserRecordsCreator()
{
	AuthCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "add"s, "user"s, NEW_USER, NEW_PASSWORD}, ostr));
	assertTrue (_pAuthService->authenticate(NEW_USER, NEW_PASSWORD));
	assertEqual (TestAuthAdminService::ADMIN, _pAuthService->getUserAttribute(NEW_USER, "createdBy"s, ""s));
}


void AuthCommandTest::testRemoveUserRefusesSessionUser()
{
	AuthCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		command.execute(admin, {"auth"s, "rm"s, "user"s, TestAuthAdminService::ADMIN}, ostr));
	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		command.execute(admin, {"auth"s, "remove"s, "user"s, Poco::toUpper(TestAuthAdminService::ADMIN)}, ostr));
	assertTrue (_pAuthService->userExists(TestAuthAdminService::ADMIN));

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "remove"s, "user"s, TestAuthAdminService::GUEST}, ostr));
	assertFalse (_pAuthService->userExists(TestAuthAdminService::GUEST));
}


void AuthCommandTest::testReservedAttributeIsRefusedByService()
{
	AuthCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;

	try
	{
		[[maybe_unused]] const int status = command.execute(admin,
			{"auth"s, "set"s, "user"s, TestAuthAdminService::GUEST, "auth_ldap=false"s}, ostr);
		fail("setting an attribute reserved for the service must fail");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
	assertTrue (_pAuthService->getUserAttribute(TestAuthAdminService::GUEST, "auth_ldap"s, ""s).empty());
	try
	{
		[[maybe_unused]] const int status = command.execute(admin,
			{"auth"s, "unset"s, "user"s, TestAuthAdminService::GUEST, "auth_ldap"s}, ostr);
		fail("removing an attribute reserved for the service must fail");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}

	assertEqual (static_cast<int>(Command::STATUS_OK), command.execute(admin,
		{"auth"s, "set"s, "user"s, TestAuthAdminService::GUEST, "mail=guest@example.com"s}, ostr));
	assertEqual ("guest@example.com"s, _pAuthService->getUserAttribute(TestAuthAdminService::GUEST, "mail"s, ""s));
}


void AuthCommandTest::testGrantAndRevokePermissions()
{
	AuthCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;
	std::set<std::string> permissions;

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "grant"s, "user"s, TestAuthAdminService::GUEST, "read"s, "write"s}, ostr));
	_pAuthService->permissionsForUser(TestAuthAdminService::GUEST, permissions);
	assertTrue (permissions == std::set<std::string>({"read"s, "write"s}));
	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "revoke"s, "user"s, TestAuthAdminService::GUEST, "write"s}, ostr));
	_pAuthService->permissionsForUser(TestAuthAdminService::GUEST, permissions);
	assertTrue (permissions == std::set<std::string>({"read"s}));

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "add"s, "role"s, "operators"s}, ostr));
	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "grant"s, "role"s, "operators"s, "operate"s}, ostr));
	_pAuthService->permissionsForRole("operators"s, permissions);
	assertTrue (permissions == std::set<std::string>({"operate"s}));
	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "revoke"s, "role"s, "operators"s, "operate"s}, ostr));
	_pAuthService->permissionsForRole("operators"s, permissions);
	assertTrue (permissions.empty());

	assertEqual (static_cast<int>(Command::STATUS_USAGE),
		command.execute(admin, {"auth"s, "grant"s, "user"s, TestAuthAdminService::GUEST}, ostr));
	assertEqual (static_cast<int>(Command::STATUS_USAGE),
		command.execute(admin, {"auth"s, "revoke"s, "group"s, "operators"s, "operate"s}, ostr));
}


void AuthCommandTest::testUnlockUnlocksUser()
{
	AuthCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;
	_pAuthService->lockOutUser(TestAuthAdminService::GUEST, Poco::DateTime());

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"auth"s, "unlock"s, TestAuthAdminService::GUEST}, ostr));
	assertFalse (_pAuthService->isUserLockedOut(TestAuthAdminService::GUEST));
}


void AuthCommandTest::testShowUsersWithAttributeNeedsValue()
{
	AuthCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;

	try
	{
		[[maybe_unused]] const int status = command.execute(admin, {"auth"s, "show"s, "users"s, "attr:mail"s}, ostr);
		fail("an attribute condition without a value must fail");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


CppUnit::Test* AuthCommandTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthCommandTest");

	CppUnit_addTest(pSuite, AuthCommandTest, testAuthRequiresAdminPermission);
	CppUnit_addTest(pSuite, AuthCommandTest, testAddUserNeedsNameAndPassword);
	CppUnit_addTest(pSuite, AuthCommandTest, testAddUserRecordsCreator);
	CppUnit_addTest(pSuite, AuthCommandTest, testRemoveUserRefusesSessionUser);
	CppUnit_addTest(pSuite, AuthCommandTest, testReservedAttributeIsRefusedByService);
	CppUnit_addTest(pSuite, AuthCommandTest, testGrantAndRevokePermissions);
	CppUnit_addTest(pSuite, AuthCommandTest, testUnlockUnlocksUser);
	CppUnit_addTest(pSuite, AuthCommandTest, testShowUsersWithAttributeNeedsValue);

	return pSuite;
}
