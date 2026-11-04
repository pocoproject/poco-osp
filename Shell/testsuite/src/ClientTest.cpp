//
// ClientTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ClientTest.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Properties.h"
#include "Poco/FileStream.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include "Poco/Process.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#if defined(POCO_OS_FAMILY_WINDOWS)
#include "Poco/Environment.h"
#include "Poco/String.h"
#endif
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <vector>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");

	int writeArguments(Session&, const std::vector<std::string>& args, std::ostream& ostr)
	{
		for (std::size_t i = 1; i < args.size(); ++i)
		{
			ostr << args[i] << '\n';
		}
		return Command::STATUS_OK;
	}

	std::string option(const std::string& name, const std::string& value = ""s)
		/// Returns the option in the syntax of Poco::Util::Application on the platform.
	{
#if defined(POCO_OS_FAMILY_WINDOWS)
		std::string text = "/"s + name;
#else
		std::string text = "--"s + name;
#endif
		if (!value.empty()) text += "="s + value;
		return text;
	}
}


ClientTest::ClientTest(const std::string& name): CppUnit::TestCase(name)
{
}


ClientTest::~ClientTest() = default;


void ClientTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
	_pFixture->addCommand("say"s, ""s, writeArguments);
	_pFixture->addCommand("adminsay"s, TestAuthService::ADMIN_PERMISSION, writeArguments);
}


void ClientTest::tearDown()
{
	_pFixture.reset();
}


ClientTest::Output ClientTest::runClient(const std::vector<std::string>& args, const Poco::Process::Env& env)
{
	Poco::Pipe inPipe;
	Poco::Pipe outPipe;
	Poco::Pipe errPipe;
#if defined(POCO_OS_FAMILY_WINDOWS)
	// On Windows, the environment given to the client replaces that of the
	// test, and a program without SystemRoot cannot use sockets.
	Poco::Process::Env clientEnv(env);
	if (!clientEnv.empty()) clientEnv.emplace("SystemRoot"s, Poco::Environment::get("SystemRoot"s));
#else
	const Poco::Process::Env& clientEnv = env;
#endif
	Poco::ProcessHandle handle =
		Poco::Process::launch(OSPSHELL_TEST_CLIENT, args, &inPipe, &outPipe, &errPipe, clientEnv);
	inPipe.close();
	Output output;
	Poco::PipeInputStream outStream(outPipe);
	Poco::StreamCopier::copyToString(outStream, output.standard);
	Poco::PipeInputStream errStream(errPipe);
	Poco::StreamCopier::copyToString(errStream, output.error);
#if defined(POCO_OS_FAMILY_WINDOWS)
	// The client writes text, whose lines end with CR LF on Windows.
	Poco::replaceInPlace(output.standard, "\r\n"s, "\n"s);
	Poco::replaceInPlace(output.error, "\r\n"s, "\n"s);
#endif
	assertEqual (0, handle.wait());
	return output;
}


void ClientTest::testHelpNamesHostAndPortOptions()
{
	const std::string help = runClient({option("help"s)}).standard;
#if defined(POCO_OS_FAMILY_WINDOWS)
	assertTrue (help.find("[<option> ...] [<command>]"s) != std::string::npos);
	// Windows has no short options.
	assertTrue (help.find("/host=<host>"s) != std::string::npos);
	assertTrue (help.find("/port=<port>"s) != std::string::npos);
	assertTrue (help.find("/password=<password>"s) != std::string::npos);
#else
	assertTrue (help.find("[<option> ...] [-- <command>]"s) != std::string::npos);
	assertTrue (help.find("-H <host>, --host=<host>"s) != std::string::npos);
	assertTrue (help.find("-P <port>, --port=<port>"s) != std::string::npos);
	assertTrue (help.find("-p <password>, --password=<password>"s) != std::string::npos);
#endif
}


void ClientTest::testControlCharactersFromServerAreReplaced()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const std::string output =
		runClient({option("noauth"s), option("host"s, "127.0.0.1"s), option("port"s, port), "say"s,
			"red\x1b[31mtext\abell\ttab \xC3\xA4"s}).standard;
	assertEqual ("red?[31mtext?bell\ttab \xC3\xA4\n"s, output);
}


void ClientTest::testCommandFileWithoutFinalNewline()
{
	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		ostr << "say first\nsay second";
	}
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const std::string output =
		runClient({option("noauth"s), option("host"s, "127.0.0.1"s), option("port"s, port),
			option("file"s, commandFile.path())}).standard;
	assertEqual ("first\nsecond\n"s, output);
}


// The prompt for the user name and the password reads from a terminal,
// which the tests do not have, so only the other ways to log in are tested.


void ClientTest::testLoginWithOptions()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::ADMIN),
		option("password"s, TestAuthService::ADMIN_PASSWORD), option("host"s, "127.0.0.1"s), option("port"s, port),
		"adminsay"s, "done"s});
	assertEqual ("done\n"s, output.standard);
	assertEqual (""s, output.error);
}


void ClientTest::testLoginWithEnvironmentVariables()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Poco::Process::Env env{
		{"OSPSH_USERNAME"s, TestAuthService::ADMIN},
		{"OSPSH_PASSWORD"s, TestAuthService::ADMIN_PASSWORD}};
	const Output output = runClient({option("host"s, "127.0.0.1"s), option("port"s, port), "adminsay"s, "done"s}, env);
	assertEqual ("done\n"s, output.standard);
	assertEqual (""s, output.error);
}


void ClientTest::testRejectedLoginIsReported()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::ADMIN), option("password"s, "wrong"s),
		option("host"s, "127.0.0.1"s), option("port"s, port), "adminsay"s, "done"s});
	assertEqual (""s, output.standard);
	assertEqual ("Not authorized.\n"s, output.error);
}


CppUnit::Test* ClientTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ClientTest");

	CppUnit_addTest(pSuite, ClientTest, testHelpNamesHostAndPortOptions);
	CppUnit_addTest(pSuite, ClientTest, testControlCharactersFromServerAreReplaced);
	CppUnit_addTest(pSuite, ClientTest, testCommandFileWithoutFinalNewline);
	CppUnit_addTest(pSuite, ClientTest, testLoginWithOptions);
	CppUnit_addTest(pSuite, ClientTest, testLoginWithEnvironmentVariables);
	CppUnit_addTest(pSuite, ClientTest, testRejectedLoginIsReported);

	return pSuite;
}
