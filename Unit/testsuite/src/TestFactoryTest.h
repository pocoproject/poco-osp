//
// TestFactoryTest.h
//
// Definition of the TestFactoryTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestFactoryTest_INCLUDED
#define TestFactoryTest_INCLUDED


#include "Poco/OSP/Unit/Unit.h"
#include "CppUnit/TestCase.h"


class TestFactoryTest: public CppUnit::TestCase
{
public:
	TestFactoryTest() = delete;
	explicit TestFactoryTest(const std::string& name);
	~TestFactoryTest() override;

	void testTestsAreKeyedBySymbolicName();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // TestFactoryTest_INCLUDED
