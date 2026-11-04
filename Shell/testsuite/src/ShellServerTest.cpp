//
// ShellServerTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ShellServerTest.h"
#include "ShellClient.h"
#include "ShellFixture.h"
#include "LogCapture.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Event.h"
#include "Poco/Message.h"
#include "Poco/Stopwatch.h"
#include "Poco/Thread.h"
#include "Poco/Types.h"
#include <atomic>
#include <chrono>
#include <cstddef>
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
	constexpr long WAIT_MS = 20000;
	constexpr std::chrono::seconds STOP_TIMEOUT{10};
}


ShellServerTest::ShellServerTest(const std::string& name): CppUnit::TestCase(name)
{
}


ShellServerTest::~ShellServerTest() = default;


void ShellServerTest::setUp()
{
	_entered.reset();
	_released.reset();
	_commandEnded = false;
	_pFixture = std::make_unique<ShellFixture>(""s);
	_pFixture->addCommand("block"s, ""s, [this](Session&, const std::vector<std::string>&, std::ostream&)
	{
		_entered.set();
		const bool released = _released.tryWait(WAIT_MS);
		_commandEnded = true;
		return released ? Command::STATUS_OK : Command::STATUS_ERROR;
	});
}


void ShellServerTest::tearDown()
{
	_released.set();
	_pFixture.reset();
}


void ShellServerTest::testStopWaitsForSessionInCommand()
{
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	client.sendLine("block"s);
	assertTrue (_entered.tryWait(WAIT_MS));

	// The command ends only after stop() has shut down the session socket,
	// so stop() returns 0 only if it waits for the session.
	std::atomic<bool> closedByServer{false};
	Poco::Thread releaser;
	releaser.startFunc([this, &client, &closedByServer]()
	{
		closedByServer = client.closedByServer();
		_released.set();
	});
	const std::size_t activeSessions = _pFixture->server().stop(STOP_TIMEOUT);
	const bool commandEnded = _commandEnded;
	releaser.join();
	assertEqual (static_cast<std::size_t>(0), activeSessions);
	assertTrue (commandEnded);
	assertTrue (closedByServer);
}


void ShellServerTest::testStopEndsIdleSession()
{
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);

	assertEqual (static_cast<std::size_t>(0), _pFixture->stopServer(STOP_TIMEOUT));
	assertTrue (client.closedByServer());
}


void ShellServerTest::testStopReturnsAfterTimeoutForSessionInCommand()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	ShellClient client(_pFixture->startServer());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	client.sendLine("block"s);
	assertTrue (_entered.tryWait(WAIT_MS));

	const auto timeout = std::chrono::milliseconds(500);
	Poco::Stopwatch stopwatch;
	stopwatch.start();
	assertEqual (static_cast<std::size_t>(1), _pFixture->stopServer(timeout));
	stopwatch.stop();
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) >= timeout - std::chrono::milliseconds(50));
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) < STOP_TIMEOUT);

	bool warned = false;
	for (const auto& msg: capture.channel().waitForMessages(0))
	{
		warned = warned || (msg.getPriority() == Poco::Message::PRIO_WARNING &&
			msg.getText().find("1 shell sessions have not ended"s) == 0);
	}
	assertTrue (warned);

	_released.set();
	assertEqual (static_cast<std::size_t>(0), _pFixture->stopServer(STOP_TIMEOUT));
}


void ShellServerTest::testStopClosesServerSocketForSessionInCommand()
{
	const Poco::UInt16 port = _pFixture->startServer();
	ShellClient client(port);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	client.sendLine("block"s);
	assertTrue (_entered.tryWait(WAIT_MS));
	assertEqual (static_cast<std::size_t>(1), _pFixture->stopServer(std::chrono::milliseconds(100)));

	bool refused = false;
	try
	{
		const Poco::Net::StreamSocket socket(Poco::Net::SocketAddress("127.0.0.1"s, port));
	}
	catch (Poco::Net::ConnectionRefusedException&)
	{
		refused = true;
	}
	assertTrue (refused);

	_released.set();
	assertEqual (static_cast<std::size_t>(0), _pFixture->stopServer(STOP_TIMEOUT));
}


CppUnit::Test* ShellServerTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ShellServerTest");

	CppUnit_addTest(pSuite, ShellServerTest, testStopWaitsForSessionInCommand);
	CppUnit_addTest(pSuite, ShellServerTest, testStopEndsIdleSession);
	CppUnit_addTest(pSuite, ShellServerTest, testStopReturnsAfterTimeoutForSessionInCommand);
	CppUnit_addTest(pSuite, ShellServerTest, testStopClosesServerSocketForSessionInCommand);

	return pSuite;
}
