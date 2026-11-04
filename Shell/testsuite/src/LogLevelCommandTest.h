//
// LogLevelCommandTest.h
//
// Definition of the LogLevelCommandTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LogLevelCommandTest_INCLUDED
#define LogLevelCommandTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class ShellFixture;


class LogLevelCommandTest: public CppUnit::TestCase
	/// Tests the loglevel command of the loglevel command bundle.
{
public:
	LogLevelCommandTest() = delete;
	explicit LogLevelCommandTest(const std::string& name);
	~LogLevelCommandTest() override;

	void setUp() override;
	void tearDown() override;

	void testLogLevelRequiresPermission();
	void testAdministratorChangesLevel();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // LogLevelCommandTest_INCLUDED
