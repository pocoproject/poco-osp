//
// SSHShellConnectionTest.h
//
// Definition of the SSHShellConnectionTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SSHShellConnectionTest_INCLUDED
#define SSHShellConnectionTest_INCLUDED


#include "CppUnit/TestCase.h"
#include "Poco/OSP/Shell/SessionTracker.h"
#include "Poco/SSH/SSHServer.h"
#include "Poco/Net/SocketDefs.h"
#include "Poco/Event.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Thread.h"
#include "Poco/Types.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>


class ShellFixture;


class SSHShellConnectionTest: public CppUnit::TestCase
	/// Tests the SSH sessions of the shell with an SSH server and client of Poco::SSH.
{
public:
	SSHShellConnectionTest() = delete;
	explicit SSHShellConnectionTest(const std::string& name);
	~SSHShellConnectionTest() override;

	void setUp() override;
	void tearDown() override;

	void testRejectedPasswordLoginsAreLoggedAndClose();
	void testPasswordLoginsFailedByAuthServiceAreLoggedAndClose();
	void testLoginCommandIsThrottled();
	void testPasswordLoginAuthorizesCommandsUnderAccountName();
	void testUserNameThatIsNoFileNameEndsSession();
	void testIdleSessionIsClosed();
	void testLongCommandLineEndsSession();
	void testCommandLineKeepsUTF8Text();
	void testStdExceptionFromCommandIsServerError();
	void testSessionWithoutCompleteCommandLineIsClosed();
	void testStopEndsWaitingSession();
	void testStopReturnsAfterTimeoutForSessionInCommand();
	void testConnectionsWithoutLoginAreClosed();
	void testConnectionWithoutShellSessionIsClosed();
#if !defined(POCO_OS_FAMILY_WINDOWS)
	void testDuplicateDescriptorsAreNotInherited();
#endif

	static CppUnit::Test* suite();

private:
	static constexpr std::chrono::seconds DEFAULT_LOGIN_GRACE_TIME{60};

	Poco::UInt16 startServer(std::chrono::seconds sessionTimeout,
		std::chrono::seconds loginGraceTime = DEFAULT_LOGIN_GRACE_TIME);
		/// Starts an SSH server with shell sessions on 127.0.0.1 and returns its port.

	std::unique_ptr<ShellFixture> _pFixture;
	Poco::TemporaryFile _hostKeyDirectory;
	Poco::OSP::Shell::SessionTracker _sessions;
	std::unique_ptr<Poco::SSH::SSHServer> _pServer;
	Poco::Thread _stopper;
	Poco::Event _entered;
	Poco::Event _released;
	std::atomic<poco_socket_t> _sessionDescriptor{POCO_INVALID_SOCKET};
		/// The descriptor that libssh has for the last connection of the server.
	Poco::Event _connectionCreated;
};


#endif // SSHShellConnectionTest_INCLUDED
