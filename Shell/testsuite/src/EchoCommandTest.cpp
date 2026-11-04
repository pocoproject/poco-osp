//
// EchoCommandTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "EchoCommandTest.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Properties.h"
#include <memory>
#include <sstream>
#include <string>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	const std::string ECHO_BUNDLE("com.appinf.osp.samples.echocommand");
}


EchoCommandTest::EchoCommandTest(const std::string& name): CppUnit::TestCase(name)
{
}


EchoCommandTest::~EchoCommandTest() = default;


void EchoCommandTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
	_pFixture->addCommandBundle(ECHO_BUNDLE, "../samples/EchoCommand/bundle"s, OSPSHELL_TEST_ECHO_LIBRARY);
}


void EchoCommandTest::tearDown()
{
	_pFixture.reset();
}


void EchoCommandTest::testEchoRequiresLogin()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_DENIED), _pFixture->processor().execute(session, "echo one"s, ostr));
	assertTrue (ostr.str().empty());
}


void EchoCommandTest::testEchoWritesArgumentsUnchanged()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_OK),
		_pFixture->processor().execute(session, "echo ${application.dir} two"s, ostr));
	assertEqual ("${application.dir} two\n"s, ostr.str());
}


CppUnit::Test* EchoCommandTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("EchoCommandTest");

	CppUnit_addTest(pSuite, EchoCommandTest, testEchoRequiresLogin);
	CppUnit_addTest(pSuite, EchoCommandTest, testEchoWritesArgumentsUnchanged);

	return pSuite;
}
