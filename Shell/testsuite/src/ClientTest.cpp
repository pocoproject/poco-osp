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
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Bugcheck.h"
#include "Poco/Event.h"
#include "Poco/Exception.h"
#include "Poco/FileStream.h"
#include "Poco/Net/DialogSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnection.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include "Poco/Process.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Util/Application.h"
#if defined(POCO_OS_FAMILY_WINDOWS)
#include "Poco/String.h"
#endif
#include <chrono>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <thread>
#include <vector>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	constexpr std::chrono::seconds CLIENT_TIMEOUT{60};
		/// The time after which a client that has not ended is killed.

	class ProcessWatchdog
		/// Kills a process that has not ended within the given time, so that
		/// a test that waits for the process fails instead of waiting forever.
	{
	public:
		ProcessWatchdog(Poco::ProcessHandle& handle, std::chrono::milliseconds timeout):
			_thread([this, &handle, timeout]()
				{
					if (!_ended.tryWait(static_cast<long>(timeout.count())))
					{
						try
						{
							Poco::Process::kill(handle);
						}
						catch (const Poco::Exception&)
						{
							// The process has ended in the meantime.
						}
					}
				})
		{
		}

		ProcessWatchdog(const ProcessWatchdog&) = delete;
		ProcessWatchdog(ProcessWatchdog&&) = delete;
		ProcessWatchdog& operator=(const ProcessWatchdog&) = delete;
		ProcessWatchdog& operator=(ProcessWatchdog&&) = delete;

		~ProcessWatchdog()
		{
			try
			{
				_ended.set();
				_thread.join();
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

	private:
		Poco::Event _ended;
		std::thread _thread;
	};

	class GreetingCutConnection: public Poco::Net::TCPServerConnection
		/// Sends the first line of a greeting of two lines and closes the connection.
	{
	public:
		using Poco::Net::TCPServerConnection::TCPServerConnection;

		void run() override
		{
			Poco::Net::DialogSocket dialog(socket());
			dialog.sendMessage("200-Welcome"s);
		}
	};

	class ReplyCutConnection: public Poco::Net::TCPServerConnection
		/// Sends a greeting, answers the first request with the first line
		/// of a reply of two lines and closes the connection.
	{
	public:
		using Poco::Net::TCPServerConnection::TCPServerConnection;

		void run() override
		{
			Poco::Net::DialogSocket dialog(socket());
			dialog.sendMessage("200 Welcome"s);
			std::string request;
			if (dialog.receiveMessage(request)) dialog.sendMessage("100-first line"s);
		}
	};

	int writeArguments(Session&, const std::vector<std::string>& args, std::ostream& ostr)
	{
		for (std::size_t i = 1; i < args.size(); ++i)
		{
			ostr << args[i] << '\n';
		}
		return Command::STATUS_OK;
	}

	int writeLongLine(Session&, const std::vector<std::string>& /*args*/, std::ostream& ostr)
	{
		// Longer than the longest line that ospsh accepts (Poco::Net::DialogSocket::MAX_LINE_LENGTH).
		constexpr std::size_t LONG_LINE_LENGTH = 5000;
		ostr << std::string(LONG_LINE_LENGTH, 'x') << '\n';
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


ClientTest::Output ClientTest::runClient(
	const std::vector<std::string>& args, const Poco::Process::Env& env, const std::string& input)
{
	Poco::Pipe inPipe;
	Poco::Pipe outPipe;
	Poco::Pipe errPipe;
	Poco::ProcessHandle handle =
		Poco::Process::launch(OSPSHELL_TEST_CLIENT, args, &inPipe, &outPipe, &errPipe, env);
	const ProcessWatchdog watchdog(handle, CLIENT_TIMEOUT);
	Poco::PipeOutputStream inStream(inPipe);
	inStream << input;
	inStream.close();
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
	output.exitCode = handle.wait();
	return output;
}


void ClientTest::testHelpNamesHostAndPortOptions()
{
	const Output output = runClient({option("help"s)});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	const std::string& help = output.standard;
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
	const Output output =
		runClient({option("username"s, TestAuthService::ADMIN), option("password"s, TestAuthService::ADMIN_PASSWORD),
			option("host"s, "127.0.0.1"s), option("port"s, port), "say"s, "red\x1b[31mtext\abell\ttab \xC3\xA4"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual ("red?[31mtext?bell\ttab \xC3\xA4\n"s, output.standard);
}


void ClientTest::testCommandFileWithoutFinalNewline()
{
	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		ostr << "login " << TestAuthService::ADMIN << ' ' << TestAuthService::ADMIN_PASSWORD
			<< "\nsay first\nsay second";
	}
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output =
		runClient({option("noauth"s), option("host"s, "127.0.0.1"s), option("port"s, port),
			option("file"s, commandFile.path())});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual ("first\nsecond\n"s, output.standard);
}


// The prompt for the user name and the password reads from a terminal,
// which the tests do not have, so only the other ways to log in are tested.


void ClientTest::testLoginWithOptions()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::ADMIN),
		option("password"s, TestAuthService::ADMIN_PASSWORD), option("host"s, "127.0.0.1"s), option("port"s, port),
		"adminsay"s, "done"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual (""s, output.error);
	assertEqual ("done\n"s, output.standard);
}


void ClientTest::testLoginWithEnvironmentVariables()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Poco::Process::Env env{
		{"OSPSH_USERNAME"s, TestAuthService::ADMIN},
		{"OSPSH_PASSWORD"s, TestAuthService::ADMIN_PASSWORD}};
	const Output output = runClient({option("host"s, "127.0.0.1"s), option("port"s, port), "adminsay"s, "done"s}, env);
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual (""s, output.error);
	assertEqual ("done\n"s, output.standard);
}


void ClientTest::testRejectedLoginIsReported()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::ADMIN), option("password"s, "wrong"s),
		option("host"s, "127.0.0.1"s), option("port"s, port), "adminsay"s, "done"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_NOPERM), output.exitCode);
	assertEqual ("Not authorized.\n"s, output.error);
	assertEqual (""s, output.standard);
}


void ClientTest::testCredentialsAndArgumentsKeepQuotesAndBackslashes()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::QUOTE_USER),
		option("password"s, TestAuthService::QUOTE_PASSWORD), option("host"s, "127.0.0.1"s), option("port"s, port),
		"say"s, "a\\b"s, "\"'"s, "end\\"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual (""s, output.error);
	assertEqual ("a\\b\n\"'\nend\\\n"s, output.standard);
}


void ClientTest::testUserNamedDoubleDashLogsIn()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::DOUBLE_DASH_USER),
		option("password"s, TestAuthService::DOUBLE_DASH_PASSWORD), option("host"s, "127.0.0.1"s),
		option("port"s, port), "say"s, "done"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual (""s, output.error);
	assertEqual ("done\n"s, output.standard);
}


void ClientTest::testLoginCommandSkipsLeadingDoubleDash()
{
	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		ostr << "login -- " << TestAuthService::DOUBLE_DASH_USER << ' ' << TestAuthService::DOUBLE_DASH_PASSWORD
			<< "\nsay done\n";
	}
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("noauth"s), option("host"s, "127.0.0.1"s), option("port"s, port),
		option("file"s, commandFile.path())});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual (""s, output.error);
	assertEqual ("done\n"s, output.standard);
}


void ClientTest::testLoginCommandShowsHelpOnlyForSingleOption()
{
	// A user may be named "-h", so with a password, "-h" is a user name.
	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		ostr << "login -h wrong\n";
	}
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("noauth"s), option("host"s, "127.0.0.1"s), option("port"s, port),
		option("file"s, commandFile.path())});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_SOFTWARE), output.exitCode);
	assertEqual ("Not authorized.\n"s, output.error);
	assertEqual (""s, output.standard);
}


void ClientTest::testPasswdCommandEndsOptionsBeforeUserName()
{
	_pFixture->addCommand("passwd"s, ""s, writeArguments);
	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		ostr << "passwd -h secret\npasswd -- --help secret\n";
	}
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::ADMIN),
		option("password"s, TestAuthService::ADMIN_PASSWORD), option("host"s, "127.0.0.1"s), option("port"s, port),
		option("file"s, commandFile.path())});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), output.exitCode);
	assertEqual (""s, output.error);
	assertEqual ("--\n-h\nsecret\n--\n--help\nsecret\n"s, output.standard);
}


void ClientTest::testExitCodeShowsFailedLastCommand()
{
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const std::vector<std::string> options{option("username"s, TestAuthService::GUEST),
		option("password"s, TestAuthService::GUEST_PASSWORD), option("host"s, "127.0.0.1"s), option("port"s, port)};
	std::vector<std::string> args(options);
	args.insert(args.end(), {"adminsay"s, "done"s});
	const Output denied = runClient(args);
	assertEqual ("Not authorized.\n"s, denied.error);
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_SOFTWARE), denied.exitCode);

	// Blank lines after the last command do not change the exit code.
	Poco::TemporaryFile deniedFile;
	{
		Poco::FileOutputStream ostr(deniedFile.path());
		ostr << "say first\nadminsay second\n\n   \n";
	}
	args = options;
	args.push_back(option("file"s, deniedFile.path()));
	const Output deniedLast = runClient(args);
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_SOFTWARE), deniedLast.exitCode);
	assertEqual ("first\n"s, deniedLast.standard);

	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		ostr << "adminsay first\nsay second\n";
	}
	args = options;
	args.push_back(option("file"s, commandFile.path()));
	const Output succeeded = runClient(args);
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_OK), succeeded.exitCode);
	assertEqual ("second\n"s, succeeded.standard);
}


void ClientTest::testPasswordsThatDoNotMatchFailPasswd()
{
	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		ostr << "passwd\n";
	}
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	// passwd reads the new password and its confirmation from the standard input.
	const Output output = runClient({option("username"s, TestAuthService::ADMIN),
		option("password"s, TestAuthService::ADMIN_PASSWORD), option("host"s, "127.0.0.1"s), option("port"s, port),
		option("file"s, commandFile.path())}, {}, "first\nsecond\n"s);
	assertEqual ("Passwords do not match.\n"s, output.error);
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_SOFTWARE), output.exitCode);
}


void ClientTest::testLongReplyLineIsNotReportedAsClosedConnection()
{
	_pFixture->addCommand("long"s, ""s, writeLongLine);
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("username"s, TestAuthService::ADMIN),
		option("password"s, TestAuthService::ADMIN_PASSWORD), option("host"s, "127.0.0.1"s), option("port"s, port),
		"long"s});
	assertTrue (output.error.find("The server has closed the connection."s) == std::string::npos);
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_SOFTWARE), output.exitCode);
}


void ClientTest::testConnectionClosedByServerIsReported()
{
	// The server closes the connection after the third rejected login.
	Poco::TemporaryFile commandFile;
	{
		Poco::FileOutputStream ostr(commandFile.path());
		for (int i = 0; i < Poco::OSP::Shell::ShellConnection::MAX_FAILED_LOGINS; ++i)
		{
			ostr << "login " << TestAuthService::ADMIN << " wrong\n";
		}
		ostr << "say done\n";
	}
	const std::string port = Poco::NumberFormatter::format(_pFixture->startServer());
	const Output output = runClient({option("noauth"s), option("host"s, "127.0.0.1"s), option("port"s, port),
		option("file"s, commandFile.path())});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_UNAVAILABLE), output.exitCode);
	assertTrue (output.error.find("The server has closed the connection."s) != std::string::npos);
	assertEqual (""s, output.standard);
}


void ClientTest::testConnectionClosedWithinGreetingIsReported()
{
	Poco::Net::TCPServer server(new Poco::Net::TCPServerConnectionFactoryImpl<GreetingCutConnection>,
		Poco::Net::ServerSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0)));
	server.start();
	const Output output = runClient({option("noauth"s), option("host"s, "127.0.0.1"s),
		option("port"s, Poco::NumberFormatter::format(server.port())), "say"s, "done"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_UNAVAILABLE), output.exitCode);
	assertEqual ("The server has closed the connection.\n"s, output.error);
	assertEqual (""s, output.standard);
}


void ClientTest::testConnectionClosedWithinReplyIsReported()
{
	Poco::Net::TCPServer server(new Poco::Net::TCPServerConnectionFactoryImpl<ReplyCutConnection>,
		Poco::Net::ServerSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0)));
	server.start();
	const Output output = runClient({option("noauth"s), option("host"s, "127.0.0.1"s),
		option("port"s, Poco::NumberFormatter::format(server.port())), "say"s, "done"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_UNAVAILABLE), output.exitCode);
	assertEqual ("The server has closed the connection.\n"s, output.error);
	assertEqual (""s, output.standard);
}


void ClientTest::testConfiguredPortOutOfRangeIsRefused()
{
	// The port of the running server plus 65536, which a conversion to
	// 16 bits would turn into the port of the server.
	const int port = _pFixture->startServer() + 65536;
	const std::string configPath = Poco::TemporaryFile::tempName() + ".properties";
	Poco::TemporaryFile::registerForDeletion(configPath);
	{
		Poco::FileOutputStream ostr(configPath);
		ostr << "host = 127.0.0.1\nport = " << port << '\n';
	}
	const Output output = runClient({option("noauth"s), option("config-file"s, configPath), "say"s, "done"s});
	assertEqual (static_cast<int>(Poco::Util::Application::EXIT_CONFIG), output.exitCode);
	assertEqual ("The port "s + Poco::NumberFormatter::format(port) + " is not between 1 and 65535.\n"s, output.error);
	assertEqual (""s, output.standard);
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
	CppUnit_addTest(pSuite, ClientTest, testCredentialsAndArgumentsKeepQuotesAndBackslashes);
	CppUnit_addTest(pSuite, ClientTest, testUserNamedDoubleDashLogsIn);
	CppUnit_addTest(pSuite, ClientTest, testLoginCommandSkipsLeadingDoubleDash);
	CppUnit_addTest(pSuite, ClientTest, testLoginCommandShowsHelpOnlyForSingleOption);
	CppUnit_addTest(pSuite, ClientTest, testPasswdCommandEndsOptionsBeforeUserName);
	CppUnit_addTest(pSuite, ClientTest, testExitCodeShowsFailedLastCommand);
	CppUnit_addTest(pSuite, ClientTest, testPasswordsThatDoNotMatchFailPasswd);
	CppUnit_addTest(pSuite, ClientTest, testLongReplyLineIsNotReportedAsClosedConnection);
	CppUnit_addTest(pSuite, ClientTest, testConnectionClosedByServerIsReported);
	CppUnit_addTest(pSuite, ClientTest, testConnectionClosedWithinGreetingIsReported);
	CppUnit_addTest(pSuite, ClientTest, testConnectionClosedWithinReplyIsReported);
	CppUnit_addTest(pSuite, ClientTest, testConfiguredPortOutOfRangeIsRefused);

	return pSuite;
}
