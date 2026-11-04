//
// SSHShellConnectionTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SSHShellConnectionTest.h"
#include "ShellFixture.h"
#include "TestAuthService.h"
#include "LogCapture.h"
#include "SSHShellConnection.h"
#include "ShellConnectionSupport.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Properties.h"
#include "Poco/SSH/SSHClient.h"
#include "Poco/SSH/SSHHostKeyManager.h"
#include "Poco/SSH/SSHServer.h"
#include "Poco/SSH/SSHServerConfig.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketDefs.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Exception.h"
#include "Poco/Message.h"
#include "Poco/Path.h"
#include "Poco/Stopwatch.h"
#include "Poco/Thread.h"
#include "Poco/Types.h"
#include <libssh/libssh.h>
#if !defined(POCO_OS_FAMILY_WINDOWS)
#include <fcntl.h>
#include <sys/socket.h>
#endif
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;
using Poco::OSP::Shell::SSHShellConnection;
using Poco::OSP::Shell::ShellConnection;
using Poco::SSH::SSHClient;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	constexpr std::chrono::seconds MAX_WAIT{10};
	constexpr long WAIT_MS = static_cast<long>(std::chrono::milliseconds(MAX_WAIT).count());
	constexpr int READ_TIMEOUT_MS = 100;
	constexpr std::uint32_t READ_BUFFER_SIZE = 256;
	constexpr int MAX_CONNECTIONS = 4;

	bool readUntil(ssh_channel channel, const std::string& marker)
		/// Reads from the channel until the received text contains marker.
		/// Returns false if the channel ends or the wait times out before.
	{
		const auto deadline = std::chrono::steady_clock::now() + MAX_WAIT;
		std::string received;
		std::array<char, READ_BUFFER_SIZE> buffer{};
		while (received.find(marker) == std::string::npos)
		{
			if (std::chrono::steady_clock::now() >= deadline) return false;
			const int length = ssh_channel_read_timeout(channel, buffer.data(), READ_BUFFER_SIZE, 0, READ_TIMEOUT_MS);
			if (length == SSH_ERROR || ssh_channel_is_eof(channel) != 0) return false;
			if (length > 0) received.append(buffer.data(), static_cast<std::size_t>(length));
		}
		return true;
	}

	bool waitForEnd(ssh_channel channel, std::chrono::milliseconds timeout = MAX_WAIT)
		/// Reads from the channel until the server ends it. Returns false if
		/// the channel stays open longer than the given time.
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		std::array<char, READ_BUFFER_SIZE> buffer{};
		while (std::chrono::steady_clock::now() < deadline)
		{
			const int length = ssh_channel_read_timeout(channel, buffer.data(), READ_BUFFER_SIZE, 0, READ_TIMEOUT_MS);
			if (length == SSH_ERROR || ssh_channel_is_eof(channel) != 0 || ssh_channel_is_open(channel) == 0)
				return true;
		}
		return false;
	}

	[[noreturn]] int throwStdException(Session&, const std::vector<std::string>&, std::ostream&)
	{
		throw std::runtime_error("test failure"s);
	}

	void writeLine(ssh_channel channel, const std::string& line)
	{
		const std::string data = line + "\r";
		ssh_channel_write(channel, data.data(), static_cast<uint32_t>(data.size()));
	}

	std::vector<Poco::Message> warnings(CaptureChannel& channel)
	{
		std::vector<Poco::Message> result;
		for (const auto& msg: channel.waitForMessages(0))
		{
			if (msg.getPriority() == Poco::Message::PRIO_WARNING) result.push_back(msg);
		}
		return result;
	}

#if !defined(POCO_OS_FAMILY_WINDOWS)
	constexpr int MAX_SCANNED_DESCRIPTOR = 1024;
		/// The system gives out the lowest free descriptor, so those of the
		/// test process are below this limit.
	constexpr long POLL_INTERVAL_MS = 10;
		/// The interval in which a test checks again for a state it waits for.

	std::vector<int> descriptorsConnectedTo(const Poco::Net::SocketAddress& peer, int excluded)
		/// Returns the descriptors of the process, other than excluded, that
		/// are sockets connected to the given address. For a connection to the
		/// SSH server, these are the descriptor of libssh, the duplicate of the
		/// shell and the duplicates that the server keeps for itself.
	{
		std::vector<int> result;
		for (int fd = 0; fd < MAX_SCANNED_DESCRIPTOR; ++fd)
		{
			struct sockaddr_storage address{};
			poco_socklen_t length = sizeof(address);
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the socket API takes a generic address
			auto* pAddress = reinterpret_cast<struct sockaddr*>(&address);
			if (fd != excluded && ::getpeername(fd, pAddress, &length) == 0 &&
				Poco::Net::SocketAddress(pAddress, length) == peer)
			{
				result.push_back(fd);
			}
		}
		return result;
	}
#endif
}


SSHShellConnectionTest::SSHShellConnectionTest(const std::string& name): CppUnit::TestCase(name)
{
}


SSHShellConnectionTest::~SSHShellConnectionTest() = default;


void SSHShellConnectionTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
}


void SSHShellConnectionTest::tearDown()
{
	_released.set();
	_stopper.join();
	if (_pServer != nullptr)
	{
		_sessions.stop();
		_pServer->stop();
		_pServer.reset();
	}
	_pFixture.reset();
}


Poco::UInt16 SSHShellConnectionTest::startServer(std::chrono::seconds sessionTimeout,
	std::chrono::seconds loginGraceTime)
{
	Poco::Path directory(_hostKeyDirectory.path());
	directory.makeDirectory();
	Poco::SSH::SSHServerConfig config;
	config.bindAddress = "127.0.0.1"s;
	config.port = ShellFixture::freePort();
	config.hostKeyPath = Poco::SSH::SSHHostKeyManager::ensureHostKey(directory.toString());
	config.enablePubkeyAuth = false;
	config.maxConnections = MAX_CONNECTIONS;
	// More attempts than the shell allows, so that the tests see the limit of the shell.
	config.maxAuthAttempts = 2 * ShellConnection::MAX_FAILED_LOGINS;
	const ShellConnection::Config shellConfig = _pFixture->connectionConfig(sessionTimeout);
	_pServer = std::make_unique<Poco::SSH::SSHServer>(config,
		[this, shellConfig, loginGraceTime](
			ssh_session session, const Poco::SSH::SSHServerConfig& cfg, Poco::SSH::SSHServer& server)
			-> Poco::SSH::SSHSession*
		{
			auto pConnection =
				std::make_unique<SSHShellConnection>(session, cfg, server, shellConfig, _sessions, loginGraceTime);
			_sessionDescriptor = ssh_get_fd(session);
			_connectionCreated.set();
			return pConnection.release();
		});
	_pServer->start();
	return config.port;
}


void SSHShellConnectionTest::testRejectedPasswordLoginsAreLoggedAndClose()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	for (int i = 0; i < ShellConnection::MAX_FAILED_LOGINS; ++i)
	{
		assertFalse (client.authenticatePassword(TestAuthService::ADMIN, "wrong"s));
	}
	assertFalse (client.authenticatePassword(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));

	const std::vector<Poco::Message> rejected = warnings(capture.channel());
	assertEqual (static_cast<std::size_t>(ShellConnection::MAX_FAILED_LOGINS), rejected.size());
	for (const auto& msg: rejected)
	{
		assertTrue (msg.getText().find("Shell login from 127.0.0.1:"s) == 0);
	}
	assertTrue (rejected.back().getText().find("the connection is closed"s) != std::string::npos);
}


void SSHShellConnectionTest::testPasswordLoginsFailedByAuthServiceAreLoggedAndClose()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	for (int i = 1; i <= ShellConnection::MAX_FAILED_LOGINS; ++i)
	{
		// A std::exception from the service counts as a rejection, as a Poco::Exception does.
		const std::string& user = i == 2 ? TestAuthService::STD_FAILING_USER : TestAuthService::FAILING_USER;
		assertFalse (client.authenticatePassword(user, "any"s));
		// The rejection is logged before it is answered.
		assertEqual (static_cast<std::size_t>(i), warnings(capture.channel()).size());
	}
	assertFalse (client.authenticatePassword(TestAuthService::ADMIN, TestAuthService::ADMIN_PASSWORD));
}


void SSHShellConnectionTest::testLoginCommandIsThrottled()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	for (int i = 0; i < ShellConnection::MAX_FAILED_LOGINS; ++i)
	{
		writeLine(channel, "login admin wrong"s);
		assertTrue (readUntil(channel, "403 DENIED"s));
	}
	assertTrue (waitForEnd(channel));
	ssh_channel_free(channel);
	assertEqual (static_cast<std::size_t>(ShellConnection::MAX_FAILED_LOGINS), warnings(capture.channel()).size());
}


void SSHShellConnectionTest::testPasswordLoginAuthorizesCommandsUnderAccountName()
{
	_pFixture->addCommand("whoami"s, TestAuthService::ADMIN_PERMISSION,
		[](Session& session, const std::vector<std::string>&, std::ostream& ostr)
		{
			ostr << '[' << session.user() << "]\n";
			return Command::STATUS_OK;
		});
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::ADMIN_OTHER_NAME, TestAuthService::ADMIN_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	writeLine(channel, "whoami"s);
	assertTrue (readUntil(channel, "["s + TestAuthService::ADMIN + "]"s));
	ssh_channel_free(channel);
}


void SSHShellConnectionTest::testUserNameThatIsNoFileNameEndsSession()
{
	// Without an authentication service, every password login is accepted.
	_pFixture = std::make_unique<ShellFixture>(""s);
	LogCapture capture(_pFixture->shellContext()->logger().name());
	const Poco::UInt16 port = startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT);
	const std::vector<std::string> userNames{"."s, ".."s, "../keys"s, "a/b"s, "a\\b"s, "a\tb"s};
	for (const auto& userName: userNames)
	{
		SSHClient client;
		client.connect("127.0.0.1"s, port);
		assertTrue (client.authenticatePassword(userName, "any"s));
		ssh_channel channel = client.openShellChannel();
		if (channel != nullptr)
		{
			assertFalse (readUntil(channel, "osp> "s));
			ssh_channel_free(channel);
		}
	}
	assertEqual (userNames.size(), warnings(capture.channel()).size());
}


void SSHShellConnectionTest::testIdleSessionIsClosed()
{
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(std::chrono::seconds(1)));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	assertTrue (waitForEnd(channel));
	ssh_channel_free(channel);
}


void SSHShellConnectionTest::testLongCommandLineEndsSession()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	const std::string line(Poco::OSP::Shell::MAX_COMMAND_LINE_LENGTH + 1, 'x');
	ssh_channel_write(channel, line.data(), static_cast<uint32_t>(line.size()));
	assertTrue (waitForEnd(channel));
	ssh_channel_free(channel);
	const std::vector<Poco::Message> logged = warnings(capture.channel());
	assertEqual (static_cast<std::size_t>(1), logged.size());
	assertTrue (logged.front().getText().find(": a command line is longer than 4096 characters."s)
		!= std::string::npos);
}


void SSHShellConnectionTest::testCommandLineKeepsUTF8Text()
{
	_pFixture->addCommand("show"s, ""s, [](Session&, const std::vector<std::string>& args, std::ostream& ostr)
	{
		ostr << '[' << (args.size() > 1 ? args[1] : ""s) << "]\n";
		return Command::STATUS_OK;
	});
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	// "\xc5\xbe" is the UTF-8 encoding of a letter outside ASCII.
	writeLine(channel, "show \xc5\xbe"s);
	assertTrue (readUntil(channel, "[\xc5\xbe]"s));
	ssh_channel_free(channel);
}


void SSHShellConnectionTest::testStdExceptionFromCommandIsServerError()
{
	_pFixture->addCommand("fail"s, ""s, throwStdException);
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	writeLine(channel, "fail"s);
	assertTrue (readUntil(channel, "500 test failure"s));
	writeLine(channel, "help"s);
	assertTrue (readUntil(channel, "200 OK"s));
	ssh_channel_free(channel);
}


void SSHShellConnectionTest::testSessionWithoutCompleteCommandLineIsClosed()
{
	constexpr std::chrono::seconds SESSION_TIMEOUT{1};
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	// One character every half of the session timeout, without a line end.
	const auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(SESSION_TIMEOUT) / 2;
	const auto deadline = std::chrono::steady_clock::now() + 5 * SESSION_TIMEOUT;
	bool ended = false;
	while (!ended && std::chrono::steady_clock::now() < deadline)
	{
		ssh_channel_write(channel, " ", 1);
		ended = waitForEnd(channel, interval);
	}
	ssh_channel_free(channel);
	assertTrue (ended);
}


void SSHShellConnectionTest::testStopEndsWaitingSession()
{
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	assertTrue (SSHShellConnection::stopServer(*_pServer, _sessions, _stopper, MAX_WAIT));
	assertTrue (waitForEnd(channel));
	ssh_channel_free(channel);
}


void SSHShellConnectionTest::testStopReturnsAfterTimeoutForSessionInCommand()
{
	_pFixture->addCommand("block"s, ""s, [this](Session&, const std::vector<std::string>&, std::ostream&)
	{
		_entered.set();
		return _released.tryWait(WAIT_MS) ? Command::STATUS_OK : Command::STATUS_ERROR;
	});
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	assertTrue (readUntil(channel, "osp> "s));
	writeLine(channel, "block"s);
	assertTrue (_entered.tryWait(WAIT_MS));

	const auto timeout = std::chrono::milliseconds(500);
	Poco::Stopwatch stopwatch;
	stopwatch.start();
	assertFalse (SSHShellConnection::stopServer(*_pServer, _sessions, _stopper, timeout));
	stopwatch.stop();
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) >= timeout - std::chrono::milliseconds(50));
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) < MAX_WAIT);

	// The server stops once the command has ended.
	_released.set();
	assertTrue (_stopper.tryJoin(WAIT_MS));
	ssh_channel_free(channel);
}


void SSHShellConnectionTest::testConnectionsWithoutLoginAreClosed()
{
	constexpr std::chrono::seconds LOGIN_GRACE_TIME{1};
	LogCapture capture(_pFixture->shellContext()->logger().name());
	const Poco::UInt16 port = startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT, LOGIN_GRACE_TIME);

	// The connections take every slot of the server: half of them end the
	// key exchange and do not log in, the others send nothing.
	std::vector<std::unique_ptr<SSHClient>> clients;
	std::vector<Poco::Net::StreamSocket> sockets;
	clients.reserve(MAX_CONNECTIONS / 2);
	sockets.reserve(MAX_CONNECTIONS / 2);
	for (int i = 0; i < MAX_CONNECTIONS / 2; ++i)
	{
		clients.push_back(std::make_unique<SSHClient>());
		clients.back()->connect("127.0.0.1"s, port);
	}
	for (int i = 0; i < MAX_CONNECTIONS / 2; ++i)
	{
		sockets.emplace_back(Poco::Net::SocketAddress("127.0.0.1"s, port));
	}

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(MAX_CONNECTIONS);
	assertEqual (static_cast<std::size_t>(MAX_CONNECTIONS), messages.size());
	for (const auto& msg: messages)
	{
		assertTrue (msg.getText().find("Closing the SSH connection from 127.0.0.1:"s) == 0);
	}

	// A closed connection frees its slot when its session has ended, shortly
	// after the closing has been logged.
	bool loggedIn = false;
	const auto deadline = std::chrono::steady_clock::now() + MAX_WAIT;
	while (!loggedIn && std::chrono::steady_clock::now() < deadline)
	{
		try
		{
			SSHClient client;
			client.connect("127.0.0.1"s, port);
			loggedIn = client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD);
		}
		catch (Poco::Exception&)
		{
			// The server has rejected the connection, because every slot was still taken.
		}
	}
	assertTrue (loggedIn);
}


void SSHShellConnectionTest::testConnectionWithoutShellSessionIsClosed()
{
	constexpr std::chrono::seconds LOGIN_GRACE_TIME{2};
	LogCapture capture(_pFixture->shellContext()->logger().name());
	SSHClient client;
	client.connect("127.0.0.1"s, startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT, LOGIN_GRACE_TIME));
	assertTrue (client.authenticatePassword(TestAuthService::GUEST, TestAuthService::GUEST_PASSWORD));

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (static_cast<std::size_t>(1), messages.size());
	assertTrue (messages.front().getText().find("Closing the SSH connection from 127.0.0.1:"s) == 0);
	assertTrue (messages.front().getText().find(": no shell session within 2 seconds;"s) != std::string::npos);
	// The server may have answered the requests for the shell before it has
	// shut down the connection; the channel then ends at once.
	ssh_channel channel = client.openShellChannel();
	if (channel != nullptr)
	{
		assertTrue (waitForEnd(channel));
		ssh_channel_free(channel);
	}
}


#if !defined(POCO_OS_FAMILY_WINDOWS)
void SSHShellConnectionTest::testDuplicateDescriptorsAreNotInherited()
{
	const Poco::UInt16 port = startServer(ShellFixture::DEFAULT_SESSION_TIMEOUT);
	Poco::Net::StreamSocket socket(Poco::Net::SocketAddress("127.0.0.1"s, port));
	const Poco::Net::SocketAddress address = socket.address();
	assertTrue (_connectionCreated.tryWait(WAIT_MS));
	// The duplicate of the shell exists once the connection has been created.
	const std::vector<int> duplicates = descriptorsConnectedTo(address, _sessionDescriptor);
	assertFalse (duplicates.empty());
	for (const int fd: duplicates)
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX fcntl()
		assertTrue ((::fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
	}

	// Every descriptor of the connection is closed once the client has ended
	// the connection. The client only shuts down sending: closing its socket
	// with the unread greeting of the server would reset the connection, and
	// a reset socket that the server still holds no longer reports its peer.
	socket.shutdownSend();
	bool closed = false;
	const auto deadline = std::chrono::steady_clock::now() + MAX_WAIT;
	while (!closed && std::chrono::steady_clock::now() < deadline)
	{
		closed = descriptorsConnectedTo(address, -1).empty();
		if (!closed) Poco::Thread::sleep(POLL_INTERVAL_MS);
	}
	assertTrue (closed);
}
#endif


CppUnit::Test* SSHShellConnectionTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SSHShellConnectionTest");

	CppUnit_addTest(pSuite, SSHShellConnectionTest, testRejectedPasswordLoginsAreLoggedAndClose);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testPasswordLoginsFailedByAuthServiceAreLoggedAndClose);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testLoginCommandIsThrottled);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testPasswordLoginAuthorizesCommandsUnderAccountName);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testUserNameThatIsNoFileNameEndsSession);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testIdleSessionIsClosed);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testLongCommandLineEndsSession);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testCommandLineKeepsUTF8Text);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testStdExceptionFromCommandIsServerError);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testSessionWithoutCompleteCommandLineIsClosed);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testStopEndsWaitingSession);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testStopReturnsAfterTimeoutForSessionInCommand);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testConnectionsWithoutLoginAreClosed);
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testConnectionWithoutShellSessionIsClosed);
#if !defined(POCO_OS_FAMILY_WINDOWS)
	CppUnit_addTest(pSuite, SSHShellConnectionTest, testDuplicateDescriptorsAreNotInherited);
#endif

	return pSuite;
}
