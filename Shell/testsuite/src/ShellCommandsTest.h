//
// ShellCommandsTest.h
//
// Definition of the ShellCommandsTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ShellCommandsTest_INCLUDED
#define ShellCommandsTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class ShellFixture;


class ShellCommandsTest: public CppUnit::TestCase
	/// Tests the commands of the shell commands bundle.
{
public:
	ShellCommandsTest() = delete;
	explicit ShellCommandsTest(const std::string& name);
	~ShellCommandsTest() override;

	void setUp() override;
	void tearDown() override;

	void testStopOfShellBundleIsRefused();
	void testStopOfBundleRequiredByShellIsRefused();
	void testStopOfCommandsBundleIsRefused();
	void testUninstallOfShellBundleIsRefused();
	void testUpgradeOfShellBundleIsRefused();
	void testStopOfOtherBundle();
	void testStopOfBundleWithDependentsIsRefused();
	void testBundlesListsStateAndVersion();

	static CppUnit::Test* suite();

private:
	struct Result
	{
		int status = 0;
		std::string output;
	};

	Result execute(const std::string& commandLine);
		/// Executes the command line in a session of the administrator.

	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // ShellCommandsTest_INCLUDED
