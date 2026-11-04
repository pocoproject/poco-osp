//
// EchoCommandTest.h
//
// Definition of the EchoCommandTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef EchoCommandTest_INCLUDED
#define EchoCommandTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class ShellFixture;


class EchoCommandTest: public CppUnit::TestCase
	/// Tests the echo command of the EchoCommand sample.
{
public:
	EchoCommandTest() = delete;
	explicit EchoCommandTest(const std::string& name);
	~EchoCommandTest() override;

	void setUp() override;
	void tearDown() override;

	void testEchoRequiresLogin();
	void testEchoWritesArgumentsUnchanged();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // EchoCommandTest_INCLUDED
