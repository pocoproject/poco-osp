//
// LogLevelCommandTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LogLevelCommandTest.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include <memory>
#include <sstream>
#include <string>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	const std::string LOGGER_NAME("osp.shell.test.level");
}


LogLevelCommandTest::LogLevelCommandTest(const std::string& name): CppUnit::TestCase(name)
{
}


LogLevelCommandTest::~LogLevelCommandTest() = default;


void LogLevelCommandTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
	Poco::Logger::get(LOGGER_NAME).setLevel(Poco::Message::PRIO_INFORMATION);
}


void LogLevelCommandTest::tearDown()
{
	Poco::Logger::get(LOGGER_NAME).setLevel(Poco::Message::PRIO_INFORMATION);
	_pFixture.reset();
}


void LogLevelCommandTest::testLogLevelRequiresPermission()
{
	const std::string commandLine = "loglevel "s + LOGGER_NAME + " none"s;
	std::ostringstream ostr;
	Session anonymous(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		_pFixture->processor().execute(anonymous, commandLine, ostr));

	Session guest(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (guest.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	assertEqual (static_cast<int>(Command::STATUS_DENIED), _pFixture->processor().execute(guest, commandLine, ostr));
	assertEqual (static_cast<int>(Poco::Message::PRIO_INFORMATION), Poco::Logger::get(LOGGER_NAME).getLevel());
}


void LogLevelCommandTest::testAdministratorChangesLevel()
{
	Session admin(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (admin.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_OK),
		_pFixture->processor().execute(admin, "loglevel "s + LOGGER_NAME + " debug"s, ostr));
	assertEqual (static_cast<int>(Poco::Message::PRIO_DEBUG), Poco::Logger::get(LOGGER_NAME).getLevel());
}


CppUnit::Test* LogLevelCommandTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("LogLevelCommandTest");

	CppUnit_addTest(pSuite, LogLevelCommandTest, testLogLevelRequiresPermission);
	CppUnit_addTest(pSuite, LogLevelCommandTest, testAdministratorChangesLevel);

	return pSuite;
}
