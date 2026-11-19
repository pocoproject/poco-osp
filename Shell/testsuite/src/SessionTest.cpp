//
// SessionTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SessionTest.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "LogCapture.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/Service.h"
#include "Poco/Message.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>


using Poco::OSP::Shell::Session;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	const std::string MISSING_SERVICE_NAME("osp.shell.test.missing");
	const std::string OTHER_SERVICE_NAME("osp.shell.test.other");

	class OtherService: public Poco::OSP::Service
		/// A service that is not an authentication service.
	{
	public:
		OtherService() = default;

		OtherService(const OtherService&) = delete;
		OtherService(OtherService&&) = delete;
		OtherService& operator = (const OtherService&) = delete;
		OtherService& operator = (OtherService&&) = delete;

	protected:
		~OtherService() override = default;
	};
}


SessionTest::SessionTest(const std::string& name): CppUnit::TestCase(name)
{
}


SessionTest::~SessionTest() = default;


void SessionTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
	_pFixture->registry().registerService(OTHER_SERVICE_NAME, new OtherService, Poco::OSP::Properties());
}


void SessionTest::tearDown()
{
	_pFixture.reset();
}


void SessionTest::testLoginWithValidCredentials()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertFalse (session.authenticated());
	assertTrue (session.user().empty());
	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	assertTrue (session.authenticated());
	assertEqual (TestAuthService::GUEST, session.user());
}


void SessionTest::testLoginKeepsAccountName()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::ADMIN_OTHER_NAME, TestAuthService::ADMIN_PASSWORD));
	assertEqual (TestAuthService::ADMIN, session.user());
	assertTrue (session.authorize(TestAuthService::ADMIN_PERMISSION));
}


void SessionTest::testLoginWithWrongPassword()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertFalse (session.login(TestAuthService::ADMIN, "wrong"s));
	assertFalse (session.authenticated());
	assertFalse (session.authorize(""s));
	assertFalse (session.authorize(TestAuthService::ADMIN_PERMISSION));
}


void SessionTest::testRejectedLoginLeavesNoUser()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertFalse (session.login(TestAuthService::ADMIN, "wrong"s));
	assertTrue (session.user().empty());

	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	assertFalse (session.login(TestAuthService::ADMIN, "wrong"s));
	assertFalse (session.authenticated());
	assertTrue (session.user().empty());
}


void SessionTest::testAuthorizeChecksPermission()
{
	Session admin(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (admin.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	assertTrue (admin.authorize(TestAuthService::ADMIN_PERMISSION));
	assertTrue (admin.authorize(""s));
	assertFalse (admin.authorize("other"s));

	Session guest(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (guest.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	assertFalse (guest.authorize(TestAuthService::ADMIN_PERMISSION));
	assertTrue (guest.authorize(""s));
}


void SessionTest::testAuthorizeBeforeLogin()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertFalse (session.authorize(""s));
	assertFalse (session.authorize(TestAuthService::ADMIN_PERMISSION));
}


void SessionTest::testEmptyServiceNameDisablesAuthentication()
{
	Session session(_pFixture->shellContext(), ""s);
	assertTrue (session.login("anyone"s, "anything"s));
	assertTrue (session.authenticated());
	assertEqual ("anyone"s, session.user());
	assertTrue (session.authorize(TestAuthService::ADMIN_PERMISSION));
}


void SessionTest::testLoginWithKey()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	session.loginWithKey(TestAuthService::ADMIN);
	assertTrue (session.authenticated());
	assertEqual (TestAuthService::ADMIN, session.user());
	assertTrue (session.authorize(TestAuthService::ADMIN_PERMISSION));
}


void SessionTest::testMissingAuthServiceDeniesLogin()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	Session session(_pFixture->shellContext(), MISSING_SERVICE_NAME);
	assertFalse (session.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	assertFalse (session.authenticated());
	assertFalse (session.authorize(""s));
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (static_cast<std::size_t>(1), messages.size());
	assertEqual (static_cast<int>(Poco::Message::PRIO_WARNING), static_cast<int>(messages[0].getPriority()));
	assertTrue (messages[0].getText().find(MISSING_SERVICE_NAME + " is not available"s) != std::string::npos);
}


void SessionTest::testMissingAuthServiceDeniesPermissions()
{
	Session session(_pFixture->shellContext(), MISSING_SERVICE_NAME);
	session.loginWithKey(TestAuthService::ADMIN);
	assertTrue (session.authorize(""s));
	assertFalse (session.authorize(TestAuthService::ADMIN_PERMISSION));
}


void SessionTest::testUnregisteredAuthServiceIsNotUsed()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	assertTrue (session.authorize(TestAuthService::ADMIN_PERMISSION));

	_pFixture->registry().unregisterService(AUTH_SERVICE_NAME);
	assertFalse (session.authorize(TestAuthService::ADMIN_PERMISSION));
	assertFalse (session.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
}


void SessionTest::testAuthServiceExceptionRejectsLogin()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	assertFalse (session.login(TestAuthService::FAILING_USER, "any"s));
	assertFalse (session.authenticated());
	assertTrue (session.user().empty());
	assertEqual (1, session.failedLogins());
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (static_cast<std::size_t>(1), messages.size());
	assertEqual (static_cast<int>(Poco::Message::PRIO_ERROR), static_cast<int>(messages[0].getPriority()));
	assertTrue (messages[0].getText().find(AUTH_SERVICE_NAME + " could not check a shell login"s)
		!= std::string::npos);
}


void SessionTest::testStdExceptionFromAuthServiceRejectsLogin()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	assertFalse (session.login(TestAuthService::STD_FAILING_USER, "any"s));
	assertFalse (session.authenticated());
	assertTrue (session.user().empty());
	assertEqual (1, session.failedLogins());
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (static_cast<std::size_t>(1), messages.size());
	assertEqual (static_cast<int>(Poco::Message::PRIO_ERROR), static_cast<int>(messages[0].getPriority()));
	assertTrue (messages[0].getText().find(AUTH_SERVICE_NAME + " could not check a shell login"s)
		!= std::string::npos);
}


void SessionTest::testServiceOfOtherTypeRejectsLogin()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	Session session(_pFixture->shellContext(), OTHER_SERVICE_NAME);
	session.loginWithKey(TestAuthService::GUEST);
	assertFalse (session.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	assertFalse (session.authenticated());
	assertTrue (session.user().empty());
	assertEqual (1, session.failedLogins());
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (static_cast<std::size_t>(1), messages.size());
	assertEqual (static_cast<int>(Poco::Message::PRIO_ERROR), static_cast<int>(messages[0].getPriority()));
	assertTrue (messages[0].getText().find(OTHER_SERVICE_NAME + " could not check a shell login"s)
		!= std::string::npos);
}


void SessionTest::testAuthServiceExceptionDeniesPermission()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	assertFalse (session.authorize(TestAuthService::FAILING_PERMISSION));
	assertFalse (session.authorize(TestAuthService::STD_FAILING_PERMISSION));
	assertTrue (session.authenticated());
	assertTrue (session.authorize(TestAuthService::ADMIN_PERMISSION));
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(2);
	assertEqual (static_cast<std::size_t>(2), messages.size());
	for (const auto& msg: messages)
	{
		assertEqual (static_cast<int>(Poco::Message::PRIO_WARNING), static_cast<int>(msg.getPriority()));
		assertTrue (msg.getText().find(AUTH_SERVICE_NAME + " could not check the permission "s) != std::string::npos);
	}
}


void SessionTest::testServiceOfOtherTypeDeniesPermission()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	Session session(_pFixture->shellContext(), OTHER_SERVICE_NAME);
	session.loginWithKey(TestAuthService::ADMIN);
	assertFalse (session.authorize(TestAuthService::ADMIN_PERMISSION));
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (static_cast<std::size_t>(1), messages.size());
	assertEqual (static_cast<int>(Poco::Message::PRIO_WARNING), static_cast<int>(messages[0].getPriority()));
	assertTrue (messages[0].getText().find(OTHER_SERVICE_NAME + " could not check the permission "s)
		!= std::string::npos);
}


CppUnit::Test* SessionTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SessionTest");

	CppUnit_addTest(pSuite, SessionTest, testLoginWithValidCredentials);
	CppUnit_addTest(pSuite, SessionTest, testLoginKeepsAccountName);
	CppUnit_addTest(pSuite, SessionTest, testLoginWithWrongPassword);
	CppUnit_addTest(pSuite, SessionTest, testRejectedLoginLeavesNoUser);
	CppUnit_addTest(pSuite, SessionTest, testAuthorizeChecksPermission);
	CppUnit_addTest(pSuite, SessionTest, testAuthorizeBeforeLogin);
	CppUnit_addTest(pSuite, SessionTest, testEmptyServiceNameDisablesAuthentication);
	CppUnit_addTest(pSuite, SessionTest, testLoginWithKey);
	CppUnit_addTest(pSuite, SessionTest, testMissingAuthServiceDeniesLogin);
	CppUnit_addTest(pSuite, SessionTest, testMissingAuthServiceDeniesPermissions);
	CppUnit_addTest(pSuite, SessionTest, testUnregisteredAuthServiceIsNotUsed);
	CppUnit_addTest(pSuite, SessionTest, testAuthServiceExceptionRejectsLogin);
	CppUnit_addTest(pSuite, SessionTest, testStdExceptionFromAuthServiceRejectsLogin);
	CppUnit_addTest(pSuite, SessionTest, testServiceOfOtherTypeRejectsLogin);
	CppUnit_addTest(pSuite, SessionTest, testAuthServiceExceptionDeniesPermission);
	CppUnit_addTest(pSuite, SessionTest, testServiceOfOtherTypeDeniesPermission);

	return pSuite;
}
