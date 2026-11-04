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
#include "Poco/OSP/BundleEvent.h"
#include <memory>
#include <string>
#include <vector>


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
	void testStopOfBundleWithModuleDependentsIsRefused();
	void testStopAllStopsModuleDependentsFirst();
	void testBundlesListsStateAndVersion();
	void testReadCommandsAreDeniedBeforeLogin();

	static CppUnit::Test* suite();

private:
	struct Result
	{
		int status = 0;
		std::string output;
	};

	[[nodiscard]] Result execute(const std::string& commandLine);
		/// Executes the command line in a session of the administrator.

	void onBundleStopping(const void* pSender, Poco::OSP::BundleEvent& event);
		/// Records the symbolic name of the bundle that is stopping.

	std::unique_ptr<ShellFixture> _pFixture;
	std::vector<std::string> _stoppingBundles;
};


#endif // ShellCommandsTest_INCLUDED
