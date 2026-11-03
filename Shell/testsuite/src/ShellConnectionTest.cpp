//
// ShellConnectionTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ShellConnectionTest.h"
#include "ShellClient.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "LogCapture.h"
#include "ShellConnectionSupport.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Properties.h"
#include "Poco/ErrorHandler.h"
#include "Poco/Exception.h"
#include "Poco/Message.h"
#include "Poco/Stopwatch.h"
#include "Poco/Types.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <vector>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using Poco::OSP::Shell::ShellConnection;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	constexpr std::chrono::seconds MAX_WAIT{10};

	std::vector<Poco::Message> messagesWithPriority(const std::vector<Poco::Message>& messages,
		Poco::Message::Priority priority)
	{
		std::vector<Poco::Message> result;
		std::copy_if(messages.begin(), messages.end(), std::back_inserter(result), [priority](const Poco::Message& msg)
		{
			return msg.getPriority() == priority;
		});
		return result;
	}

	[[noreturn]] int throwIOException(Session&, const std::vector<std::string>&, std::ostream&)
	{
		throw Poco::IOException("test failure"s);
	}

	class CountingErrorHandler: public Poco::ErrorHandler
		/// Counts the exceptions that reach it, while it is the error handler.
	{
	public:
		CountingErrorHandler():
			_pPrevious(Poco::ErrorHandler::set(this))
		{
		}

		~CountingErrorHandler() override
		{
			Poco::ErrorHandler::set(_pPrevious);
		}

		CountingErrorHandler(const CountingErrorHandler&) = delete;
		CountingErrorHandler(CountingErrorHandler&&) = delete;
		CountingErrorHandler& operator = (const CountingErrorHandler&) = delete;
		CountingErrorHandler& operator = (CountingErrorHandler&&) = delete;

		void exception(const Poco::Exception&) override
		{
			++_count;
		}

		void exception(const std::exception&) override
		{
			++_count;
		}

		void exception() override
		{
			++_count;
		}

		[[nodiscard]] int count() const
		{
			return _count;
		}

	private:
		Poco::ErrorHandler* _pPrevious;
		std::atomic<int> _count{0};
	};
}


ShellConnectionTest::ShellConnectionTest(const std::string& name): CppUnit::TestCase(name)
{
}


ShellConnectionTest::~ShellConnectionTest() = default;


void ShellConnectionTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
}


void ShellConnectionTest::tearDown()
{
	_pFixture.reset();
}


void ShellConnectionTest::testGreetingOnConnect()
{
	ShellClient client(_pFixture->startServer());
	const ShellClient::Response greeting = client.receive();
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), greeting.status);
	assertTrue (greeting.text.find("OSP Command Shell Service ready.\n"s) == 0);
	assertEqual ("Enter \"h\" or \"help\" for a list of commands."s, greeting.final);
}


void ShellConnectionTest::testCommandResponse()
{
	_pFixture->addCommand("lines"s, ""s, [](Session&, const std::vector<std::string>&, std::ostream& ostr)
	{
		ostr << "one\ntwo\n";
		return Command::STATUS_OK;
	});
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	const ShellClient::Response response = client.send("lines"s);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), response.status);
	assertEqual ("one\ntwo\n"s, response.text);
	assertEqual ("OK"s, response.final);
}


void ShellConnectionTest::testQuitEndsSession()
{
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	const ShellClient::Response response = client.send("quit"s);
	assertEqual (static_cast<int>(ShellConnection::STATUS_GOODBYE), response.status);
	assertEqual ("GOODBYE"s, response.final);
	assertTrue (client.closedByServer());
}


void ShellConnectionTest::testExceptionFromCommandIsServerError()
{
	_pFixture->addCommand("fail"s, ""s, throwIOException);
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	assertEqual (static_cast<int>(ShellConnection::STATUS_SERVER_ERROR), client.send("fail"s).status);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.send("help"s).status);
}


void ShellConnectionTest::testLoginGrantsPermission()
{
	_pFixture->addCommand("admin"s, TestAuthService::ADMIN_PERMISSION,
		[](Session&, const std::vector<std::string>&, std::ostream&)
	{
		return Command::STATUS_OK;
	});
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	assertEqual (static_cast<int>(ShellConnection::STATUS_CLIENT_DENIED), client.send("admin"s).status);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.send("login admin secret"s).status);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.send("admin"s).status);
}


void ShellConnectionTest::testBundleStopEndsSessionWithoutError()
{
	CountingErrorHandler errorHandler;
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	_pFixture->bundle(ShellFixture::SHELL_BUNDLE)->stop();
	assertTrue (client.closedByServer());
	_pFixture->stopServer();
	assertEqual (0, errorHandler.count());
}


void ShellConnectionTest::testRejectedLoginsAreDelayedAndCloseConnection()
{
	const std::vector<Poco::Message> warnings =
		messagesWithPriority(rejectLoginsUntilClosed("login admin wrong"s), Poco::Message::PRIO_WARNING);
	assertEqual (static_cast<std::size_t>(ShellConnection::MAX_FAILED_LOGINS), warnings.size());
	for (const auto& warning: warnings)
	{
		assertTrue (warning.getText().find("Shell login from 127.0.0.1:"s) == 0);
	}
	assertTrue (warnings.back().getText().find("the connection is closed"s) != std::string::npos);
}


void ShellConnectionTest::testLoginsFailedByAuthServiceAreDelayedAndCloseConnection()
{
	const std::vector<Poco::Message> messages =
		rejectLoginsUntilClosed("login "s + TestAuthService::FAILING_USER + " any"s);
	assertEqual (static_cast<std::size_t>(ShellConnection::MAX_FAILED_LOGINS),
		messagesWithPriority(messages, Poco::Message::PRIO_WARNING).size());
	assertEqual (static_cast<std::size_t>(ShellConnection::MAX_FAILED_LOGINS),
		messagesWithPriority(messages, Poco::Message::PRIO_ERROR).size());
}


void ShellConnectionTest::testLoginsFailedByStdExceptionAreDelayedAndCloseConnection()
{
	const std::vector<Poco::Message> messages =
		rejectLoginsUntilClosed("login "s + TestAuthService::STD_FAILING_USER + " any"s);
	assertEqual (static_cast<std::size_t>(ShellConnection::MAX_FAILED_LOGINS),
		messagesWithPriority(messages, Poco::Message::PRIO_WARNING).size());
	assertEqual (static_cast<std::size_t>(ShellConnection::MAX_FAILED_LOGINS),
		messagesWithPriority(messages, Poco::Message::PRIO_ERROR).size());
}


void ShellConnectionTest::testPermissionFailedByAuthServiceIsDenied()
{
	_pFixture->addCommand("check"s, TestAuthService::FAILING_PERMISSION,
		[](Session&, const std::vector<std::string>&, std::ostream&)
	{
		return Command::STATUS_OK;
	});
	LogCapture capture(_pFixture->shellContext()->logger().name());
	{
		ShellClient client(_pFixture->startServer());
		assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
		assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.send("login admin secret"s).status);
		const ShellClient::Response response = client.send("check"s);
		assertEqual (static_cast<int>(ShellConnection::STATUS_CLIENT_DENIED), response.status);
		assertTrue (response.text.empty());
		assertEqual ("DENIED"s, response.final);
	}
	_pFixture->stopServer();
	const std::vector<Poco::Message> warnings =
		messagesWithPriority(capture.channel().waitForMessages(0), Poco::Message::PRIO_WARNING);
	assertEqual (static_cast<std::size_t>(1), warnings.size());
}


void ShellConnectionTest::testAcceptedLoginIsLogged()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	{
		ShellClient client(_pFixture->startServer());
		assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
		assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.send("login admin secret"s).status);
	}
	_pFixture->stopServer();
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(0);
	const bool logged = std::any_of(messages.begin(), messages.end(), [](const Poco::Message& msg)
	{
		return msg.getText().find("User admin logged in to the shell from 127.0.0.1:"s) == 0;
	});
	assertTrue (logged);
}


void ShellConnectionTest::testIdleSessionIsClosed()
{
	const Poco::UInt16 port = _pFixture->startServer(std::chrono::seconds(1));
	Poco::Stopwatch stopwatch;
	stopwatch.start();
	ShellClient idle(port);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), idle.receive().status);
	assertTrue (idle.closedByServer());
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) >= std::chrono::milliseconds(900));

	ShellClient next(port);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), next.receive().status);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), next.send("help"s).status);
}


void ShellConnectionTest::testSessionWithoutCompleteCommandLineIsClosed()
{
	constexpr std::chrono::seconds SESSION_TIMEOUT{1};
	ShellClient client(_pFixture->startServer(SESSION_TIMEOUT));
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	// One character every half of the session timeout, without a line end.
	const auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(SESSION_TIMEOUT) / 2;
	const auto deadline = std::chrono::steady_clock::now() + 5 * SESSION_TIMEOUT;
	bool ended = false;
	while (!ended && std::chrono::steady_clock::now() < deadline)
	{
		client.sendBytes(" "s);
		ended = client.poll(interval);
	}
	assertTrue (ended);
	assertTrue (client.closedByServer());
}


void ShellConnectionTest::testLongCommandLineEndsSession()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	const Poco::UInt16 port = _pFixture->startServer();
	const std::string longest(Poco::OSP::Shell::MAX_COMMAND_LINE_LENGTH, 'x');
	{
		ShellClient client(port);
		assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
		assertEqual (static_cast<int>(ShellConnection::STATUS_CLIENT_DENIED), client.send(longest).status);
		client.sendBytes(longest + "x"s);
		assertTrue (client.poll(MAX_WAIT));
		assertTrue (client.closedByServer());
	}
	{
		ShellClient client(port);
		assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
		client.sendLine(longest + "x"s);
		assertTrue (client.poll(MAX_WAIT));
		assertTrue (client.closedByServer());
	}

	_pFixture->stopServer();
	const std::vector<Poco::Message> errors =
		messagesWithPriority(capture.channel().waitForMessages(0), Poco::Message::PRIO_ERROR);
	assertEqual (static_cast<std::size_t>(2), errors.size());
	for (const auto& error: errors)
	{
		assertTrue (error.getText().find("Error handling connection from 127.0.0.1:"s) == 0);
	}
}


std::vector<Poco::Message> ShellConnectionTest::rejectLoginsUntilClosed(const std::string& loginLine)
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	Poco::Stopwatch stopwatch;
	stopwatch.start();
	for (int i = 0; i < ShellConnection::MAX_FAILED_LOGINS; ++i)
	{
		const ShellClient::Response response = client.send(loginLine);
		assertEqual (static_cast<int>(ShellConnection::STATUS_CLIENT_DENIED), response.status);
		assertTrue (response.text.empty());
	}
	stopwatch.stop();
	assertTrue (client.closedByServer());

	// The delays after the rejected logins from one address: one, two and four times the first delay.
	const auto expectedDelay = ShellFixture::FIRST_LOGIN_DELAY * 7;
	const auto tolerance = std::chrono::milliseconds(20);
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) >= expectedDelay - tolerance);

	_pFixture->stopServer();
	return capture.channel().waitForMessages(0);
}


CppUnit::Test* ShellConnectionTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ShellConnectionTest");

	CppUnit_addTest(pSuite, ShellConnectionTest, testGreetingOnConnect);
	CppUnit_addTest(pSuite, ShellConnectionTest, testCommandResponse);
	CppUnit_addTest(pSuite, ShellConnectionTest, testQuitEndsSession);
	CppUnit_addTest(pSuite, ShellConnectionTest, testExceptionFromCommandIsServerError);
	CppUnit_addTest(pSuite, ShellConnectionTest, testLoginGrantsPermission);
	CppUnit_addTest(pSuite, ShellConnectionTest, testBundleStopEndsSessionWithoutError);
	CppUnit_addTest(pSuite, ShellConnectionTest, testRejectedLoginsAreDelayedAndCloseConnection);
	CppUnit_addTest(pSuite, ShellConnectionTest, testLoginsFailedByAuthServiceAreDelayedAndCloseConnection);
	CppUnit_addTest(pSuite, ShellConnectionTest, testLoginsFailedByStdExceptionAreDelayedAndCloseConnection);
	CppUnit_addTest(pSuite, ShellConnectionTest, testPermissionFailedByAuthServiceIsDenied);
	CppUnit_addTest(pSuite, ShellConnectionTest, testAcceptedLoginIsLogged);
	CppUnit_addTest(pSuite, ShellConnectionTest, testIdleSessionIsClosed);
	CppUnit_addTest(pSuite, ShellConnectionTest, testSessionWithoutCompleteCommandLineIsClosed);
	CppUnit_addTest(pSuite, ShellConnectionTest, testLongCommandLineEndsSession);

	return pSuite;
}
