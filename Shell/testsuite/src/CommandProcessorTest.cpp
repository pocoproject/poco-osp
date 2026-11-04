//
// CommandProcessorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "CommandProcessorTest.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Exception.h"
#include "Poco/String.h"
#include <algorithm>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::CommandProcessor;
using Poco::OSP::Shell::Session;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");

	class TestCommandProcessor: public CommandProcessor
		/// Gives the tests access to the command line parser.
	{
	public:
		using CommandProcessor::CommandProcessor;
		using CommandProcessor::parseCommandLine;
	};

	std::vector<std::string> parse(const ShellFixture& fixture, const std::string& commandLine)
	{
		TestCommandProcessor processor(fixture.shellContext());
		std::vector<std::string> items;
		processor.parseCommandLine(commandLine, items);
		return items;
	}

	int writeArguments(Session&, const std::vector<std::string>& args, std::ostream& ostr)
	{
		for (const auto& arg: args)
		{
			ostr << '[' << arg << ']';
		}
		return Command::STATUS_OK;
	}
}


CommandProcessorTest::CommandProcessorTest(const std::string& name): CppUnit::TestCase(name)
{
}


CommandProcessorTest::~CommandProcessorTest() = default;


void CommandProcessorTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
}


void CommandProcessorTest::tearDown()
{
	_pFixture.reset();
}


void CommandProcessorTest::testExecuteDispatchesToCommand()
{
	_pFixture->addCommand("args"s, ""s, writeArguments);
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_OK),
		_pFixture->processor().execute(session, "args one \"two three\""s, ostr));
	assertEqual ("[args][one][two three]"s, ostr.str());
}


void CommandProcessorTest::testAliasRunsCommand()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(session, "h"s, ostr));
	assertTrue (ostr.str().find("The following commands are available:"s) != std::string::npos);
	assertTrue (ostr.str().find("  quit (exit)\n"s) != std::string::npos);
}


void CommandProcessorTest::testEmptyCommandLineIsAccepted()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(session, " \t "s, ostr));
	assertTrue (ostr.str().empty());
}


void CommandProcessorTest::testDuplicateCommandThrows()
{
	_pFixture->addCommand("args"s, ""s, writeArguments);
	try
	{
		_pFixture->addCommand("args"s, ""s, writeArguments);
		fail("a second command with the same name must be rejected");
	}
	catch (Poco::ExistsException&)
	{
	}
}


void CommandProcessorTest::testRemoveBundleRemovesItsCommands()
{
	_pFixture->processor().removeBundle(_pFixture->bundle(ShellFixture::COMMANDS_BUNDLE));
	std::vector<std::string> commands;
	_pFixture->processor().listCommands(commands);
	assertEqual ("help login loglevel quit"s, Poco::cat(" "s, commands.begin(), commands.end()));
}


void CommandProcessorTest::testRemoveCommand()
{
	_pFixture->addCommand("args"s, ""s, writeArguments);
	_pFixture->processor().removeCommand("args"s);
	std::vector<std::string> commands;
	_pFixture->processor().listCommands(commands);
	assertTrue (std::find(commands.begin(), commands.end(), "args"s) == commands.end());
	try
	{
		_pFixture->processor().removeCommand("args"s);
		fail("removing an unknown command must fail");
	}
	catch (Poco::NotFoundException&)
	{
	}
}


void CommandProcessorTest::testPermissionIsChecked()
{
	_pFixture->addCommand("admin"s, TestAuthService::ADMIN_PERMISSION, writeArguments);
	Session guest(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (guest.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	std::ostringstream guestOutput;
	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		_pFixture->processor().execute(guest, "admin"s, guestOutput));
	assertTrue (guestOutput.str().empty());

	Session admin(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (admin.login(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
	std::ostringstream adminOutput;
	assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(admin, "admin"s, adminOutput));
	assertEqual ("[admin]"s, adminOutput.str());
}


void CommandProcessorTest::testCommandWithoutPermissionRunsOnlyAfterLogin()
{
	_pFixture->addCommand("args"s, ""s, writeArguments);
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	std::ostringstream before;
	assertEqual (static_cast<int>(Command::STATUS_DENIED), _pFixture->processor().execute(session, "args"s, before));
	assertTrue (before.str().empty());

	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	std::ostringstream after;
	assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(session, "args"s, after));
	assertEqual ("[args]"s, after.str());
}


void CommandProcessorTest::testUnknownCommandBeforeLoginIsDenied()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_DENIED),
		_pFixture->processor().execute(session, "nosuchcommand"s, ostr));
	assertEqual (static_cast<int>(Command::STATUS_DENIED), _pFixture->processor().execute(session, "stop x"s, ostr));
	assertTrue (ostr.str().empty());
}


void CommandProcessorTest::testUnknownCommandAfterLoginThrows()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	std::ostringstream ostr;
	try
	{
		_pFixture->processor().execute(session, "nosuchcommand"s, ostr);
		fail("an unknown command must be reported");
	}
	catch (Poco::NotFoundException&)
	{
	}
}


void CommandProcessorTest::testHelpBeforeLoginListsLoginHelpAndQuit()
{
	Session session(_pFixture->shellContext(), AUTH_SERVICE_NAME);
	std::ostringstream before;
	assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(session, "help"s, before));
	assertTrue (before.str().find("  help (?, h)\n"s) != std::string::npos);
	assertTrue (before.str().find("  login\n"s) != std::string::npos);
	assertTrue (before.str().find("  quit (exit)\n"s) != std::string::npos);
	assertTrue (before.str().find("  bundles (ls, ss)\n"s) == std::string::npos);
	assertTrue (before.str().find("  stop\n"s) == std::string::npos);
	assertTrue (before.str().find("  loglevel\n"s) == std::string::npos);

	assertTrue (session.login(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	std::ostringstream after;
	assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(session, "help"s, after));
	assertTrue (after.str().find("  bundles (ls, ss)\n"s) != std::string::npos);
	assertTrue (after.str().find("  stop\n"s) != std::string::npos);
	assertTrue (after.str().find("  loglevel\n"s) != std::string::npos);
}


void CommandProcessorTest::testParseCommandLineQuotesAndEscapes()
{
	assertTrue (parse(*_pFixture, "a \"b c\" 'd e'"s) == std::vector<std::string>({"a"s, "b c"s, "d e"s}));
	assertTrue (parse(*_pFixture, "a\\ b"s) == std::vector<std::string>({"a b"s}));
	assertTrue (parse(*_pFixture, "\"a\\\"b\" 'c\\'d'"s) == std::vector<std::string>({"a\"b"s, "c'd"s}));
	assertTrue (parse(*_pFixture, "x\"y z\"w"s) == std::vector<std::string>({"xy zw"s}));
	assertTrue (parse(*_pFixture, "\"unterminated quote"s) == std::vector<std::string>({"unterminated quote"s}));
	assertTrue (parse(*_pFixture, "trailing\\"s) == std::vector<std::string>({"trailing"s}));
}


void CommandProcessorTest::testParseCommandLineSeparators()
{
	assertTrue (parse(*_pFixture, ""s).empty());
	assertTrue (parse(*_pFixture, " \t "s).empty());
	assertTrue (parse(*_pFixture, "a\tb  c \t"s) == std::vector<std::string>({"a"s, "b"s, "c"s}));
}


CppUnit::Test* CommandProcessorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("CommandProcessorTest");

	CppUnit_addTest(pSuite, CommandProcessorTest, testExecuteDispatchesToCommand);
	CppUnit_addTest(pSuite, CommandProcessorTest, testAliasRunsCommand);
	CppUnit_addTest(pSuite, CommandProcessorTest, testEmptyCommandLineIsAccepted);
	CppUnit_addTest(pSuite, CommandProcessorTest, testDuplicateCommandThrows);
	CppUnit_addTest(pSuite, CommandProcessorTest, testRemoveBundleRemovesItsCommands);
	CppUnit_addTest(pSuite, CommandProcessorTest, testRemoveCommand);
	CppUnit_addTest(pSuite, CommandProcessorTest, testPermissionIsChecked);
	CppUnit_addTest(pSuite, CommandProcessorTest, testCommandWithoutPermissionRunsOnlyAfterLogin);
	CppUnit_addTest(pSuite, CommandProcessorTest, testUnknownCommandBeforeLoginIsDenied);
	CppUnit_addTest(pSuite, CommandProcessorTest, testUnknownCommandAfterLoginThrows);
	CppUnit_addTest(pSuite, CommandProcessorTest, testHelpBeforeLoginListsLoginHelpAndQuit);
	CppUnit_addTest(pSuite, CommandProcessorTest, testParseCommandLineQuotesAndEscapes);
	CppUnit_addTest(pSuite, CommandProcessorTest, testParseCommandLineSeparators);

	return pSuite;
}
