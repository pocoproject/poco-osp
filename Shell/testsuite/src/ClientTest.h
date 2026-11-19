//
// ClientTest.h
//
// Definition of the ClientTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ClientTest_INCLUDED
#define ClientTest_INCLUDED


#include "CppUnit/TestCase.h"
#include "Poco/Process.h"
#include <memory>
#include <string>
#include <vector>


class ShellFixture;


class ClientTest: public CppUnit::TestCase
	/// Tests the shell client ospsh against the shell server of a ShellFixture.
{
public:
	ClientTest() = delete;
	explicit ClientTest(const std::string& name);
	~ClientTest() override;

	void setUp() override;
	void tearDown() override;

	void testHelpNamesHostAndPortOptions();
	void testControlCharactersFromServerAreReplaced();
	void testCommandFileWithoutFinalNewline();
	void testLoginWithOptions();
	void testLoginWithEnvironmentVariables();
	void testRejectedLoginIsReported();
	void testCredentialsAndArgumentsKeepQuotesAndBackslashes();
	void testUserNamedDoubleDashLogsIn();
	void testLoginCommandSkipsLeadingDoubleDash();
	void testLoginCommandShowsHelpOnlyForSingleOption();
	void testPasswdCommandEndsOptionsBeforeUserName();
	void testExitCodeShowsFailedLastCommand();
	void testPasswordsThatDoNotMatchFailPasswd();
	void testLongReplyLineIsNotReportedAsClosedConnection();
	void testConnectionClosedByServerIsReported();
	void testConnectionClosedWithinGreetingIsReported();
	void testConnectionClosedWithinReplyIsReported();
	void testConfiguredPortOutOfRangeIsRefused();

	static CppUnit::Test* suite();

private:
	struct Output
	{
		std::string standard;
			/// What ospsh writes to standard output.
		std::string error;
			/// What ospsh writes to standard error.
		int exitCode = 0;
			/// The exit code of ospsh.
	};

	[[nodiscard]] Output runClient(const std::vector<std::string>& args, const Poco::Process::Env& env = {},
		const std::string& input = {});
		/// Runs ospsh with the given arguments, the given additional
		/// environment variables and the given standard input and
		/// returns what it writes and its exit code.

	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // ClientTest_INCLUDED
