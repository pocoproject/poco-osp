//
// TestCaseTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestCaseTest.h"
#include "Poco/OSP/Unit/TestCaller.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestFailure.h"
#include "CppUnit/TestResult.h"
#include "CppUnit/TestRunner.h"
#include "CppUnit/TestSuite.h"
#include <string>


namespace
{
	class SetUpFixture: public CppUnit::TestCase
	{
	public:
		explicit SetUpFixture(const std::string& name);
		void setUp() override;
		void testNothing();
	};


	SetUpFixture::SetUpFixture(const std::string& name):
		CppUnit::TestCase(name)
	{
	}


	void SetUpFixture::setUp()
	{
		throw Poco::IllegalStateException("setUp() failed");
	}


	void SetUpFixture::testNothing()
	{
	}


	class ThrowingFixture: public CppUnit::TestCase
	{
	public:
		explicit ThrowingFixture(const std::string& name);
		void testThrows();
	};


	ThrowingFixture::ThrowingFixture(const std::string& name):
		CppUnit::TestCase(name)
	{
	}


	void ThrowingFixture::testThrows()
	{
		throw Poco::NotFoundException("the detail text");
	}
}


TestCaseTest::TestCaseTest(const std::string& name): CppUnit::TestCase(name)
{
}


TestCaseTest::~TestCaseTest() = default;


void TestCaseTest::testExceptionInSetUpIsReportedAsError()
{
	Poco::OSP::Unit::TestCaller<SetUpFixture> test("testNothing", &SetUpFixture::testNothing);
	CppUnit::TestResult result;
	test.run(&result);
	assertEqual (1, result.testErrors());
}


void TestCaseTest::testExceptionTextIsReported()
{
	Poco::OSP::Unit::TestCaller<ThrowingFixture> test("testThrows", &ThrowingFixture::testThrows);
	CppUnit::TestResult result;
	CppUnitPocoExceptionText(exc);
	test.run(&result, exc);
	assertEqual (1, result.testErrors());
	const std::string text(result.errors()[0]->thrownException()->what());
	assertTrue (text.find("the detail text") != std::string::npos);
}


void TestCaseTest::testLoggerErrorWithTestCallerHeader()
{
	Poco::Logger& logger = Poco::Logger::get("TestCaseTest");
	logger.setLevel(Poco::Message::PRIO_FATAL);
	logger.error("this message is below the level of the logger");
	assertFalse (logger.error());
}


void TestCaseTest::setUp()
{
}


void TestCaseTest::tearDown()
{
}


CppUnit::Test* TestCaseTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("TestCaseTest");

	CppUnit_addTest(pSuite, TestCaseTest, testExceptionInSetUpIsReportedAsError);
	CppUnit_addTest(pSuite, TestCaseTest, testExceptionTextIsReported);
	CppUnit_addTest(pSuite, TestCaseTest, testLoggerErrorWithTestCallerHeader);

	return pSuite;
}
