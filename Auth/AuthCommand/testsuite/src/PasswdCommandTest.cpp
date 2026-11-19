//
// PasswdCommandTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "PasswdCommandTest.h"
#include "PasswdCommand.h"
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
	const std::string NEW_PASSWORD("newsecret");
	const std::string DASH_USER("-h");
}


PasswdCommandTest::PasswdCommandTest(const std::string& name): CppUnit::TestCase(name)
{
}


PasswdCommandTest::~PasswdCommandTest() = default;


void PasswdCommandTest::setUp()
{
	_pBundle = std::make_unique<TemporaryBundle>(""s);
	_pAuthService = new TestAuthAdminService;
	_pBundle->registry().registerService(AuthAdminService::SERVICE_NAME, _pAuthService, Poco::OSP::Properties());
}


void PasswdCommandTest::tearDown()
{
	_pBundle.reset();
	_pAuthService.reset();
}


void PasswdCommandTest::testPasswdWithoutLoginIsDenied()
{
	PasswdCommand command(_pBundle->context());
	Session anonymous(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		command.execute(anonymous, {"passwd"s, TestAuthAdminService::GUEST, NEW_PASSWORD}, ostr));
	// The name of the user of a session without a login is empty.
	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		command.execute(anonymous, {"passwd"s, ""s, NEW_PASSWORD}, ostr));
	assertTrue (_pAuthService->authenticate(TestAuthAdminService::GUEST, TestAuthAdminService::GUEST_PASSWORD));
}


void PasswdCommandTest::testPasswdOfOtherUserNeedsAdminPermission()
{
	PasswdCommand command(_pBundle->context());
	Session guest(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (guest.login(TestAuthAdminService::GUEST, TestAuthAdminService::GUEST_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		command.execute(guest, {"passwd"s, TestAuthAdminService::ADMIN, NEW_PASSWORD}, ostr));
	assertTrue (_pAuthService->authenticate(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
}


void PasswdCommandTest::testUserChangesOwnPassword()
{
	PasswdCommand command(_pBundle->context());
	Session guest(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (guest.login(TestAuthAdminService::GUEST, TestAuthAdminService::GUEST_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(guest, {"passwd"s, TestAuthAdminService::GUEST, NEW_PASSWORD}, ostr));
	assertTrue (_pAuthService->authenticate(TestAuthAdminService::GUEST, NEW_PASSWORD));
}


void PasswdCommandTest::testAdministratorChangesPasswords()
{
	PasswdCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"passwd"s, TestAuthAdminService::ADMIN, NEW_PASSWORD}, ostr));
	assertTrue (_pAuthService->authenticate(TestAuthAdminService::ADMIN, NEW_PASSWORD));
	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"passwd"s, TestAuthAdminService::GUEST, NEW_PASSWORD}, ostr));
	assertTrue (_pAuthService->authenticate(TestAuthAdminService::GUEST, NEW_PASSWORD));
}


void PasswdCommandTest::testHelpHasNoPrompt()
{
	PasswdCommand command(_pBundle->context());
	Session guest(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (guest.login(TestAuthAdminService::GUEST, TestAuthAdminService::GUEST_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_OK), command.execute(guest, {"passwd"s, "--help"s}, ostr));
	const std::string help = ostr.str();
	assertTrue (help.find("usage: passwd [--] <username> <password>"s) != std::string::npos);
	assertTrue (help.find("prompt"s) == std::string::npos);
}


void PasswdCommandTest::testHelpNamesAccountName()
{
	PasswdCommand command(_pBundle->context());
	Session guest(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (guest.login(TestAuthAdminService::GUEST, TestAuthAdminService::GUEST_PASSWORD));
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_OK), command.execute(guest, {"passwd"s, "--help"s}, ostr));
	assertTrue (ostr.str().find("account name"s) != std::string::npos);
}


void PasswdCommandTest::testUserNameAfterDoubleDashMayStartWithDash()
{
	PasswdCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	_pAuthService->addUser(DASH_USER, TestAuthAdminService::GUEST_PASSWORD);
	std::ostringstream ostr;

	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"passwd"s, "--"s, DASH_USER, NEW_PASSWORD}, ostr));
	assertTrue (_pAuthService->authenticate(DASH_USER, NEW_PASSWORD));
}


void PasswdCommandTest::testHelpOnlyForSingleHelpArgument()
{
	PasswdCommand command(_pBundle->context());
	Session admin(_pBundle->context(), AuthAdminService::SERVICE_NAME);
	assertTrue (admin.login(TestAuthAdminService::ADMIN, TestAuthAdminService::ADMIN_PASSWORD));
	_pAuthService->addUser(DASH_USER, TestAuthAdminService::GUEST_PASSWORD);
	std::ostringstream helpStream;

	assertEqual (static_cast<int>(Command::STATUS_OK), command.execute(admin, {"passwd"s, "--help"s}, helpStream));
	assertTrue (helpStream.str().find("usage: passwd"s) != std::string::npos);

	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_OK),
		command.execute(admin, {"passwd"s, DASH_USER, NEW_PASSWORD}, ostr));
	assertTrue (ostr.str().empty());
	assertTrue (_pAuthService->authenticate(DASH_USER, NEW_PASSWORD));
}


CppUnit::Test* PasswdCommandTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("PasswdCommandTest");

	CppUnit_addTest(pSuite, PasswdCommandTest, testPasswdWithoutLoginIsDenied);
	CppUnit_addTest(pSuite, PasswdCommandTest, testPasswdOfOtherUserNeedsAdminPermission);
	CppUnit_addTest(pSuite, PasswdCommandTest, testUserChangesOwnPassword);
	CppUnit_addTest(pSuite, PasswdCommandTest, testAdministratorChangesPasswords);
	CppUnit_addTest(pSuite, PasswdCommandTest, testHelpHasNoPrompt);
	CppUnit_addTest(pSuite, PasswdCommandTest, testHelpNamesAccountName);
	CppUnit_addTest(pSuite, PasswdCommandTest, testUserNameAfterDoubleDashMayStartWithDash);
	CppUnit_addTest(pSuite, PasswdCommandTest, testHelpOnlyForSingleHelpArgument);

	return pSuite;
}
