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
#include <memory>
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


CppUnit::Test* AuthCommandTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthCommandTest");

	CppUnit_addTest(pSuite, AuthCommandTest, testAuthRequiresAdminPermission);
	CppUnit_addTest(pSuite, AuthCommandTest, testAddUserNeedsNameAndPassword);
	CppUnit_addTest(pSuite, AuthCommandTest, testAddUserRecordsCreator);

	return pSuite;
}
