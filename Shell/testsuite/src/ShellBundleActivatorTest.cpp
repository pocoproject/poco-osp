//
// ShellBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ShellBundleActivatorTest.h"
#include "ShellClient.h"
#include "ShellFixture.h"
#include "LogCapture.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#if defined(OSP_SHELL_ENABLE_SSH)
#include "Poco/OSP/PreferencesService.h"
#include "Poco/SSH/SSHClient.h"
#include "Poco/Util/MapConfiguration.h"
#endif
#include "Poco/AutoPtr.h"
#include "Poco/Event.h"
#include "Poco/Exception.h"
#include "Poco/Message.h"
#include "Poco/Stopwatch.h"
#include "Poco/Thread.h"
#include "Poco/Types.h"
#if defined(OSP_SHELL_ENABLE_SSH)
#include <libssh/libssh.h>
#if !defined(POCO_OS_FAMILY_WINDOWS)
#include <dlfcn.h>
#endif
#endif
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// GCC defines __SANITIZE_ADDRESS__ in an AddressSanitizer build; clang reports it through __has_feature.
#if defined(__SANITIZE_ADDRESS__)
#define OSPSHELL_TEST_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OSPSHELL_TEST_ASAN 1
#endif
#endif
#if defined(OSPSHELL_TEST_ASAN)
#include <sanitizer/lsan_interface.h>
#endif


using Poco::OSP::Shell::ShellConnection;
using namespace std::string_literals;


namespace
{
	const std::string AUTH_SERVICE_NAME("osp.shell.test.auth");
	const std::string ACTIVATOR_BUNDLE("osp.shell.test.activator");
	constexpr std::chrono::seconds STOP_TIMEOUT{1};
	constexpr std::chrono::seconds MAX_WAIT{10};
	constexpr long WAIT_MS = static_cast<long>(std::chrono::milliseconds(MAX_WAIT).count());
	constexpr long POLL_INTERVAL_MS = 10;

	class BlockingAuthService: public Poco::OSP::Auth::AuthService
		/// Accepts every login, but only after released() has been set.
	{
	public:
		using Ptr = Poco::AutoPtr<BlockingAuthService>;

		BlockingAuthService() = default;

		BlockingAuthService(const BlockingAuthService&) = delete;
		BlockingAuthService(BlockingAuthService&&) = delete;
		BlockingAuthService& operator = (const BlockingAuthService&) = delete;
		BlockingAuthService& operator = (BlockingAuthService&&) = delete;

		[[nodiscard]] bool authenticate(const std::string&, const std::string&) const override
		{
			_entered.set();
			return _released.tryWait(WAIT_MS);
		}

		[[nodiscard]] bool authorize(const std::string&, const std::string&) const override
		{
			return true;
		}

		[[nodiscard]] bool authorize(const std::string&, const std::string&, const std::string&) const override
		{
			return true;
		}

		[[nodiscard]] bool userExists(const std::string&) const override
		{
			return true;
		}

		[[nodiscard]] Poco::Event& entered() const
		{
			return _entered;
		}

		[[nodiscard]] Poco::Event& released() const
		{
			return _released;
		}

	protected:
		~BlockingAuthService() override = default;
			/// Reference counted: destroyed through release() only.

	private:
		mutable Poco::Event _entered;
		mutable Poco::Event _released;
	};

	bool contains(const std::vector<Poco::Message>& messages, Poco::Message::Priority priority,
		const std::string& prefix)
	{
		return std::any_of(messages.begin(), messages.end(), [priority, &prefix](const Poco::Message& msg)
		{
			return msg.getPriority() == priority && msg.getText().find(prefix) == 0;
		});
	}

#if defined(OSP_SHELL_ENABLE_SSH)
	bool waitForMessage(CaptureChannel& channel, Poco::Message::Priority priority, const std::string& prefix)
		/// Waits until a message with the given priority and prefix has been
		/// logged. Returns false if no further message arrives within the wait
		/// time of the channel before.
	{
		std::vector<Poco::Message> messages = channel.waitForMessages(0);
		while (!contains(messages, priority, prefix))
		{
			const std::size_t count = messages.size();
			messages = channel.waitForMessages(count + 1);
			if (messages.size() == count) return false;
		}
		return true;
	}

#if !defined(POCO_OS_FAMILY_WINDOWS)
	bool isLoaded(const std::string& libraryPath)
		/// Returns true if the library at the given path is loaded, without loading it.
	{
		void* pHandle = ::dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_NOLOAD);
		if (pHandle == nullptr) return false;

		::dlclose(pHandle);
		return true;
	}
#endif
#endif
}


ShellBundleActivatorTest::ShellBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


ShellBundleActivatorTest::~ShellBundleActivatorTest() = default;


void ShellBundleActivatorTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(AUTH_SERVICE_NAME);
}


void ShellBundleActivatorTest::tearDown()
{
	_pFixture.reset();
}


void ShellBundleActivatorTest::testStopReturnsAfterTimeoutAndKeepsObjectsOfSession()
{
	const BlockingAuthService::Ptr pAuthService = new BlockingAuthService;
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, pAuthService, Poco::OSP::Properties());
	const Poco::UInt16 port = ShellFixture::freePort();
	const std::string properties =
		"interface = 127.0.0.1\n"
		"port = "s + std::to_string(port) + "\n"s +
		"authServiceName = "s + AUTH_SERVICE_NAME + "\n"s +
		"stopTimeout = "s + std::to_string(STOP_TIMEOUT.count()) + "\n"s;
	Poco::OSP::Bundle::Ptr pBundle;
	{
#if defined(OSPSHELL_TEST_ASAN)
		// The shell keeps the objects that a session which has not ended uses
		// for the rest of the process: the command processor, the rejected
		// logins and the shell server, which it creates when it starts.
		// LeakSanitizer would report them as leaks.
		const __lsan::ScopedDisabler keptObjectsAreNoLeaks;
#endif
		pBundle = _pFixture->addShellBundle(ACTIVATOR_BUNDLE, properties);
	}
	LogCapture capture(_pFixture->shellContext()->contextForBundle(pBundle)->logger().name());

	ShellClient client(port);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	client.sendLine("login admin secret"s);
	assertTrue (pAuthService->entered().tryWait(WAIT_MS));

	Poco::Stopwatch stopwatch;
	stopwatch.start();
	pBundle->stop();
	stopwatch.stop();
	const auto tolerance = std::chrono::milliseconds(50);
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) >= STOP_TIMEOUT - tolerance);
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) < 5 * STOP_TIMEOUT);
	assertTrue (contains(capture.channel().waitForMessages(0), Poco::Message::PRIO_WARNING,
		"Shell sessions that have not ended within 1000 ms after the shell stopped: 1;"s));

	// The session ends when the login returns, with the objects that the shell
	// has kept. The test waits until its connection is destroyed, which is the
	// last time the session uses the framework: the connection is then no longer
	// registered for the bundle events. No event marks this, so the test polls.
	pAuthService->released().set();
	const Poco::OSP::BundleEvents& events = _pFixture->shellContext()->events();
	const auto deadline = std::chrono::steady_clock::now() + MAX_WAIT;
	while (!events.bundleStopping.empty() && std::chrono::steady_clock::now() < deadline)
	{
		Poco::Thread::sleep(POLL_INTERVAL_MS);
	}
	assertTrue (events.bundleStopping.empty());
	assertTrue (contains(capture.channel().waitForMessages(0), Poco::Message::PRIO_INFORMATION,
		"Closing connection from 127.0.0.1:"s));
}


void ShellBundleActivatorTest::testStartAfterFailedStartSucceeds()
{
	// The port is held by a socket without SO_REUSEADDR and SO_REUSEPORT, so
	// the shell server cannot bind it.
	Poco::Net::ServerSocket portHolder;
	portHolder.bind(Poco::Net::SocketAddress("127.0.0.1"s, 0), false, false);
	portHolder.listen();
	const Poco::UInt16 port = portHolder.address().port();
	const std::string properties =
		"interface = 127.0.0.1\n"
		"port = "s + std::to_string(port) + "\n"s;
	try
	{
		(void) _pFixture->addShellBundle(ACTIVATOR_BUNDLE, properties);
		fail("the shell must not start on a port that is in use");
	}
	catch (Poco::Net::NetException&)
	{
	}
#if defined(POCO_OS_FAMILY_WINDOWS)
	catch (Poco::IOException&)
	{
		// SO_EXCLUSIVEADDRUSE of the port holder: WSAEACCES, "Permission denied".
	}
#endif
	Poco::OSP::Bundle::Ptr pBundle = _pFixture->bundle(ACTIVATOR_BUNDLE);
	assertEqual (static_cast<int>(Poco::OSP::Bundle::BUNDLE_RESOLVED), static_cast<int>(pBundle->state()));

	portHolder.close();
	pBundle->start();
	ShellClient client(port);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.receive().status);
	// An extension point left from the failed start would remove the commands
	// of a stopping bundle from the command processor that the start has replaced.
	_pFixture->bundle(ShellFixture::OTHER_BUNDLE)->stop();
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), client.send("help"s).status);
	pBundle->stop();
}


void ShellBundleActivatorTest::testOutOfRangeSettingFailsStart()
{
	const std::string validSettings = "interface = 127.0.0.1\nport = "s
		+ std::to_string(ShellFixture::freePort()) + "\n"s;
	const std::vector<std::string> settings{
		"port = 65536"s, "port = 0"s, "maxConnections = 0"s, "sessionTimeout = -1"s, "sessionTimeout = 86401"s,
		"stopTimeout = -1"s, "stopTimeout = 86401"s};
	for (const auto& setting: settings)
	{
		ShellFixture fixture(AUTH_SERVICE_NAME);
		Poco::OSP::Bundle::Ptr pBundle;
		try
		{
			pBundle = fixture.addShellBundle(ACTIVATOR_BUNDLE, validSettings + setting);
		}
		catch (Poco::InvalidArgumentException&)
		{
			continue;
		}
		pBundle->stop();
		failmsg(setting + " has not failed the start"s);
	}

#if defined(OSP_SHELL_ENABLE_SSH)
	const std::vector<std::pair<std::string, int>> sshSettings{
		{"openSSH.server.port"s, 65536}, {"openSSH.server.maxConnections"s, 0},
		{"openSSH.server.loginGraceTime"s, -1}, {"openSSH.server.loginGraceTime"s, 86401}};
	for (const auto& [name, value]: sshSettings)
	{
		ShellFixture fixture(AUTH_SERVICE_NAME);
		Poco::AutoPtr<Poco::Util::MapConfiguration> pConfig = new Poco::Util::MapConfiguration;
		pConfig->setBool("openSSH.server.enabled"s, true);
		pConfig->setString("openSSH.server.interface"s, "127.0.0.1"s);
		pConfig->setUInt("openSSH.server.port"s, ShellFixture::freePort());
		pConfig->setInt(name, value);
		fixture.registry().registerService(Poco::OSP::PreferencesService::SERVICE_NAME,
			new Poco::OSP::PreferencesService(fixture.shellContext()->persistentDirectory(), pConfig.get()),
			Poco::OSP::Properties());
		Poco::OSP::Bundle::Ptr pBundle;
		try
		{
			pBundle = fixture.addShellBundle(ACTIVATOR_BUNDLE, ""s);
		}
		catch (Poco::InvalidArgumentException&)
		{
			continue;
		}
		pBundle->stop();
		failmsg(name + " = "s + std::to_string(value) + " has not failed the start"s);
	}
#endif
}


#if defined(OSP_SHELL_ENABLE_SSH)
void ShellBundleActivatorTest::testSSHStopReturnsAfterTimeoutAndKeepsObjectsOfSession()
{
	const BlockingAuthService::Ptr pAuthService = new BlockingAuthService;
	_pFixture->registry().registerService(AUTH_SERVICE_NAME, pAuthService, Poco::OSP::Properties());
	const Poco::UInt16 port = ShellFixture::freePort();
	Poco::AutoPtr<Poco::Util::MapConfiguration> pConfig = new Poco::Util::MapConfiguration;
	pConfig->setBool("openSSH.server.enabled"s, true);
	pConfig->setString("openSSH.server.interface"s, "127.0.0.1"s);
	pConfig->setUInt("openSSH.server.port"s, port);
	// The shell reads only the global configuration, so the directory of the
	// preferences is never written.
	const Poco::OSP::PreferencesService::Ptr pPreferences =
		new Poco::OSP::PreferencesService(_pFixture->shellContext()->persistentDirectory(), pConfig.get());
	_pFixture->registry().registerService(
		Poco::OSP::PreferencesService::SERVICE_NAME, pPreferences, Poco::OSP::Properties());
	const std::string properties =
		"authServiceName = "s + AUTH_SERVICE_NAME + "\n"s +
		"stopTimeout = "s + std::to_string(STOP_TIMEOUT.count()) + "\n"s;
	Poco::OSP::Bundle::Ptr pBundle;
	{
#if defined(OSPSHELL_TEST_ASAN)
		// The shell keeps the objects that a session which has not ended uses
		// for the rest of the process: the command processor, the rejected
		// logins, the SSH server and its session tracker, which it creates when
		// it starts. LeakSanitizer would report them as leaks.
		const __lsan::ScopedDisabler keptObjectsAreNoLeaks;
#endif
		pBundle = _pFixture->addShellBundle(ACTIVATOR_BUNDLE, properties);
	}
	LogCapture capture(_pFixture->shellContext()->contextForBundle(pBundle)->logger().name());
	LogCapture serverCapture("Poco.SSH.SSHServer"s);

	// The SSH password login passes at once; the login command then waits.
	pAuthService->released().set();
	Poco::SSH::SSHClient client;
	client.connect("127.0.0.1"s, port);
	assertTrue (client.authenticatePassword("admin"s, "secret"s));
	assertTrue (pAuthService->entered().tryWait(WAIT_MS));
	ssh_channel channel = client.openShellChannel();
	assertTrue (channel != nullptr);
	const std::string loginLine("login admin secret\r"s);
	ssh_channel_write(channel, loginLine.data(), static_cast<Poco::UInt32>(loginLine.size()));
	assertTrue (pAuthService->entered().tryWait(WAIT_MS));

	Poco::Stopwatch stopwatch;
	stopwatch.start();
	{
#if defined(OSPSHELL_TEST_ASAN)
		// The shell also keeps the thread that stops the SSH server, which it
		// creates when it stops.
		const __lsan::ScopedDisabler keptThreadIsNoLeak;
#endif
		pBundle->stop();
	}
	stopwatch.stop();
	const auto tolerance = std::chrono::milliseconds(50);
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) >= STOP_TIMEOUT - tolerance);
	assertTrue (std::chrono::microseconds(stopwatch.elapsed()) < 5 * STOP_TIMEOUT);
	assertTrue (contains(capture.channel().waitForMessages(0), Poco::Message::PRIO_WARNING,
		"SSH shell sessions have not ended within 1000 ms"s));

	// The server stops when the login returns and the session has ended, with
	// the objects that the shell has kept. It logs this as the last step of
	// its stop.
	pAuthService->released().set();
	assertTrue (waitForMessage(serverCapture.channel(), Poco::Message::PRIO_INFORMATION, "SSH server stopped"s));
	ssh_channel_free(channel);
	const std::string libraryPath = _pFixture->shellContext()->pathForLibrary(ACTIVATOR_BUNDLE);
	assertTrue (contains(capture.channel().waitForMessages(0), Poco::Message::PRIO_INFORMATION,
		"The library "s + libraryPath + " of the shell bundle stays loaded"s));

#if !defined(POCO_OS_FAMILY_WINDOWS)
	// The bundle loader unloads the library of the bundle when it uninstalls
	// the bundle; the code of the kept objects stays loaded.
	pBundle->uninstall();
	assertTrue (isLoaded(libraryPath));
#endif
}
#endif


CppUnit::Test* ShellBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ShellBundleActivatorTest");

	CppUnit_addTest(pSuite, ShellBundleActivatorTest, testStopReturnsAfterTimeoutAndKeepsObjectsOfSession);
	CppUnit_addTest(pSuite, ShellBundleActivatorTest, testStartAfterFailedStartSucceeds);
	CppUnit_addTest(pSuite, ShellBundleActivatorTest, testOutOfRangeSettingFailsStart);
#if defined(OSP_SHELL_ENABLE_SSH)
	CppUnit_addTest(pSuite, ShellBundleActivatorTest, testSSHStopReturnsAfterTimeoutAndKeepsObjectsOfSession);
#endif

	return pSuite;
}
