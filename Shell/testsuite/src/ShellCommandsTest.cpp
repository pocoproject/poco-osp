//
// ShellCommandsTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ShellCommandsTest.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/Properties.h"
#include "Poco/NumberFormatter.h"
#include <memory>
#include <sstream>
#include <string>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using Poco::OSP::Bundle;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	const std::string DEPENDENT_BUNDLE("osp.shell.test.dependent");
}


ShellCommandsTest::ShellCommandsTest(const std::string& name): CppUnit::TestCase(name)
{
}


ShellCommandsTest::~ShellCommandsTest() = default;


void ShellCommandsTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
}


void ShellCommandsTest::tearDown()
{
	_pFixture.reset();
}


ShellCommandsTest::Result ShellCommandsTest::execute(const std::string& commandLine)
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	std::ostringstream ostr;
	Result result;
	result.status = _pFixture->processor().execute(session, commandLine, ostr);
	result.output = ostr.str();
	return result;
}


void ShellCommandsTest::testStopOfShellBundleIsRefused()
{
	for (const auto& commandLine: {"stop "s, "stop --force "s, "stop --all "s})
	{
		const Result result = execute(commandLine + ShellFixture::SHELL_BUNDLE);
		assertEqual (static_cast<int>(Command::STATUS_ERROR), result.status);
		assertEqual ("Cannot stop bundle com.appinf.osp.shell from a shell session: the shell requires it.\n"s,
			result.output);
		assertTrue (_pFixture->bundle(ShellFixture::SHELL_BUNDLE)->isActive());
		assertTrue (_pFixture->bundle(ShellFixture::COMMANDS_BUNDLE)->isActive());
	}
}


void ShellCommandsTest::testStopOfBundleRequiredByShellIsRefused()
{
	const Result result = execute("stop --force "s + ShellFixture::BASE_BUNDLE);
	assertEqual (static_cast<int>(Command::STATUS_ERROR), result.status);
	assertTrue (result.output.find("Cannot stop bundle "s + ShellFixture::BASE_BUNDLE + " from a shell session"s)
		== 0);
	assertTrue (_pFixture->bundle(ShellFixture::BASE_BUNDLE)->isActive());
}


void ShellCommandsTest::testStopOfCommandsBundleIsRefused()
{
	const Result result = execute("stop "s + ShellFixture::COMMANDS_BUNDLE);
	assertEqual (static_cast<int>(Command::STATUS_ERROR), result.status);
	assertTrue (result.output.find("Cannot stop bundle "s + ShellFixture::COMMANDS_BUNDLE + " from a shell session"s)
		== 0);
	assertTrue (_pFixture->bundle(ShellFixture::COMMANDS_BUNDLE)->isActive());
}


void ShellCommandsTest::testUninstallOfShellBundleIsRefused()
{
	const Result result = execute("uninstall "s + ShellFixture::SHELL_BUNDLE);
	assertEqual (static_cast<int>(Command::STATUS_ERROR), result.status);
	assertEqual ("Cannot uninstall bundle com.appinf.osp.shell from a shell session: the shell requires it.\n"s,
		result.output);
	assertTrue (_pFixture->bundle(ShellFixture::SHELL_BUNDLE)->isActive());
}


void ShellCommandsTest::testUpgradeOfShellBundleIsRefused()
{
	const Result result = execute("upgrade --force "s + ShellFixture::SHELL_BUNDLE + " file:///nonexistent.bndl"s);
	assertEqual (static_cast<int>(Command::STATUS_ERROR), result.status);
	assertEqual ("Cannot upgrade bundle com.appinf.osp.shell from a shell session: the shell requires it.\n"s,
		result.output);
	assertTrue (_pFixture->bundle(ShellFixture::SHELL_BUNDLE)->isActive());
}


void ShellCommandsTest::testStopOfOtherBundle()
{
	const Result result = execute("stop "s + ShellFixture::OTHER_BUNDLE);
	assertEqual (static_cast<int>(Command::STATUS_OK), result.status);
	assertTrue (result.output.empty());
	assertEqual (static_cast<int>(Bundle::BUNDLE_RESOLVED),
		static_cast<int>(_pFixture->bundle(ShellFixture::OTHER_BUNDLE)->state()));
}


void ShellCommandsTest::testStopOfBundleWithDependentsIsRefused()
{
	_pFixture->addBundle(DEPENDENT_BUNDLE, ShellFixture::OTHER_BUNDLE);
	const Result result = execute("stop "s + ShellFixture::OTHER_BUNDLE + " "s + DEPENDENT_BUNDLE);
	assertEqual (static_cast<int>(Command::STATUS_ERROR), result.status);
	assertTrue (result.output.find("Cannot stop bundle "s + ShellFixture::OTHER_BUNDLE
		+ " as the following bundles depend on it:\n"s + DEPENDENT_BUNDLE + " ("s) == 0);
	assertTrue (_pFixture->bundle(ShellFixture::OTHER_BUNDLE)->isActive());
	// The bundles after the refused one are left unchanged.
	assertTrue (_pFixture->bundle(DEPENDENT_BUNDLE)->isActive());
}


void ShellCommandsTest::testBundlesListsStateAndVersion()
{
	assertEqual (static_cast<int>(Command::STATUS_OK), execute("stop "s + ShellFixture::OTHER_BUNDLE).status);
	const Result result = execute("bundles osp.shell.test.*"s);
	assertEqual (static_cast<int>(Command::STATUS_OK), result.status);
	const std::string expected =
		"    ID  State         Bundle\n"s +
		Poco::NumberFormatter::format(_pFixture->bundle(ShellFixture::BASE_BUNDLE)->id(), 6) +
		"  active        osp.shell.test.base/1.0.0\n"s +
		Poco::NumberFormatter::format(_pFixture->bundle(ShellFixture::OTHER_BUNDLE)->id(), 6) +
		"  resolved      osp.shell.test.other/1.0.0\n"s;
	assertEqual (expected, result.output);
}


CppUnit::Test* ShellCommandsTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ShellCommandsTest");

	CppUnit_addTest(pSuite, ShellCommandsTest, testStopOfShellBundleIsRefused);
	CppUnit_addTest(pSuite, ShellCommandsTest, testStopOfBundleRequiredByShellIsRefused);
	CppUnit_addTest(pSuite, ShellCommandsTest, testStopOfCommandsBundleIsRefused);
	CppUnit_addTest(pSuite, ShellCommandsTest, testUninstallOfShellBundleIsRefused);
	CppUnit_addTest(pSuite, ShellCommandsTest, testUpgradeOfShellBundleIsRefused);
	CppUnit_addTest(pSuite, ShellCommandsTest, testStopOfOtherBundle);
	CppUnit_addTest(pSuite, ShellCommandsTest, testStopOfBundleWithDependentsIsRefused);
	CppUnit_addTest(pSuite, ShellCommandsTest, testBundlesListsStateAndVersion);

	return pSuite;
}
