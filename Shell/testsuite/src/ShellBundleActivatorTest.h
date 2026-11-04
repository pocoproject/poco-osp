//
// ShellBundleActivatorTest.h
//
// Definition of the ShellBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ShellBundleActivatorTest_INCLUDED
#define ShellBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class ShellFixture;


class ShellBundleActivatorTest: public CppUnit::TestCase
	/// Tests the shell bundle with its activator, which runs its own shell server.
{
public:
	ShellBundleActivatorTest() = delete;
	explicit ShellBundleActivatorTest(const std::string& name);
	~ShellBundleActivatorTest() override;

	void setUp() override;
	void tearDown() override;

	void testStopReturnsAfterTimeoutAndKeepsObjectsOfSession();
#if defined(OSP_SHELL_ENABLE_SSH)
	void testSSHStopReturnsAfterTimeoutAndKeepsObjectsOfSession();
#endif

	static CppUnit::Test* suite();

private:
	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // ShellBundleActivatorTest_INCLUDED
