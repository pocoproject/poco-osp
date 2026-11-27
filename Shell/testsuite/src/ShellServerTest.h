//
// ShellServerTest.h
//
// Definition of the ShellServerTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ShellServerTest_INCLUDED
#define ShellServerTest_INCLUDED


#include "CppUnit/TestCase.h"
#include "Poco/Event.h"
#include <atomic>
#include <memory>
#include <string>


class ShellFixture;


class ShellServerTest: public CppUnit::TestCase
{
public:
	ShellServerTest() = delete;
	explicit ShellServerTest(const std::string& name);
	~ShellServerTest() override;

	void setUp() override;
	void tearDown() override;

	void testStopWaitsForSessionInCommand();
	void testStopEndsIdleSession();
	void testStopReturnsAfterTimeoutForSessionInCommand();
	void testStopClosesServerSocketForSessionInCommand();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<ShellFixture> _pFixture;
	Poco::Event _entered;
	Poco::Event _released;
	std::atomic<bool> _commandEnded{false};
};


#endif // ShellServerTest_INCLUDED
