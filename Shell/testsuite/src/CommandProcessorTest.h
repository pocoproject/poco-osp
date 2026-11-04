//
// CommandProcessorTest.h
//
// Definition of the CommandProcessorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef CommandProcessorTest_INCLUDED
#define CommandProcessorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class ShellFixture;


class CommandProcessorTest: public CppUnit::TestCase
{
public:
	CommandProcessorTest() = delete;
	explicit CommandProcessorTest(const std::string& name);
	~CommandProcessorTest() override;

	void setUp() override;
	void tearDown() override;

	void testExecuteDispatchesToCommand();
	void testAliasRunsCommand();
	void testEmptyCommandLineIsAccepted();
	void testDuplicateCommandThrows();
	void testRemoveBundleRemovesItsCommands();
	void testRemoveCommand();
	void testPermissionIsChecked();
	void testCommandWithoutPermissionRunsOnlyAfterLogin();
	void testUnknownCommandBeforeLoginIsDenied();
	void testUnknownCommandAfterLoginThrows();
	void testHelpBeforeLoginListsLoginHelpAndQuit();
	void testParseCommandLineQuotesAndEscapes();
	void testParseCommandLineSeparators();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // CommandProcessorTest_INCLUDED
