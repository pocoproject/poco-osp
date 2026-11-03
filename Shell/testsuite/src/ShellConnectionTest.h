//
// ShellConnectionTest.h
//
// Definition of the ShellConnectionTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ShellConnectionTest_INCLUDED
#define ShellConnectionTest_INCLUDED


#include "CppUnit/TestCase.h"
#include "Poco/Message.h"
#include <memory>
#include <string>
#include <vector>


class ShellFixture;


class ShellConnectionTest: public CppUnit::TestCase
{
public:
	ShellConnectionTest() = delete;
	explicit ShellConnectionTest(const std::string& name);
	~ShellConnectionTest() override;

	void setUp() override;
	void tearDown() override;

	void testGreetingOnConnect();
	void testCommandResponse();
	void testQuitEndsSession();
	void testExceptionFromCommandIsServerError();
	void testLoginGrantsPermission();
	void testBundleStopEndsSessionWithoutError();
	void testRejectedLoginsAreDelayedAndCloseConnection();
	void testLoginsFailedByAuthServiceAreDelayedAndCloseConnection();
	void testLoginsFailedByStdExceptionAreDelayedAndCloseConnection();
	void testPermissionFailedByAuthServiceIsDenied();
	void testAcceptedLoginIsLogged();
	void testIdleSessionIsClosed();
	void testSessionWithoutCompleteCommandLineIsClosed();
	void testLongCommandLineEndsSession();

	static CppUnit::Test* suite();

private:
	[[nodiscard]] std::vector<Poco::Message> rejectLoginsUntilClosed(const std::string& loginLine);
		/// Sends the login command line MAX_FAILED_LOGINS times over a new
		/// connection, checks that each login is rejected after the delay and
		/// that the server then closes the connection, and returns the
		/// messages that the shell has logged.

	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // ShellConnectionTest_INCLUDED
