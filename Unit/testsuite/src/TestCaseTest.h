//
// TestCaseTest.h
//
// Definition of the TestCaseTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestCaseTest_INCLUDED
#define TestCaseTest_INCLUDED


#include "Poco/OSP/Unit/Unit.h"
#include "CppUnit/TestCase.h"


class TestCaseTest: public CppUnit::TestCase
{
public:
	TestCaseTest() = delete;
	explicit TestCaseTest(const std::string& name);
	~TestCaseTest() override;

	void testExceptionInSetUpIsReportedAsError();
	void testExceptionTextIsReported();
	void testLoggerErrorWithTestCallerHeader();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // TestCaseTest_INCLUDED
