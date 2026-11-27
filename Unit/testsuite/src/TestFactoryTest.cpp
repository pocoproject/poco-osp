//
// TestFactoryTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestFactoryTest.h"
#include "Poco/OSP/Unit/TestFactory.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <string>


using Poco::OSP::Unit::TestFactory;


TestFactoryTest::TestFactoryTest(const std::string& name): CppUnit::TestCase(name)
{
}


TestFactoryTest::~TestFactoryTest() = default;


void TestFactoryTest::testTestsAreKeyedBySymbolicName()
{
	TestFactory factory;
	CppUnit::Test* pFirst = new CppUnit::TestSuite("First");
	CppUnit::Test* pSecond = new CppUnit::TestSuite("Second");
	CppUnit::Test* pOther = new CppUnit::TestSuite("Other");
	factory.add("com.example.tested", pFirst);
	factory.add("com.example.tested", pSecond);
	factory.add("com.example.other", pOther);

	TestFactory::TestVec tests = factory.get("com.example.tested");
	assertEqual (2, tests.size());
	assertTrue (tests.at(0).get() == pFirst);
	assertTrue (tests.at(1).get() == pSecond);
	tests = factory.get("com.example.other");
	assertEqual (1, tests.size());
	assertTrue (tests.at(0).get() == pOther);

	factory.remove("com.example.tested");
	assertTrue (factory.get("com.example.tested").empty());
	assertEqual (1, factory.get("com.example.other").size());
	assertTrue (factory.get("com.example.unknown").empty());
}


void TestFactoryTest::setUp()
{
}


void TestFactoryTest::tearDown()
{
}


CppUnit::Test* TestFactoryTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("TestFactoryTest");

	CppUnit_addTest(pSuite, TestFactoryTest, testTestsAreKeyedBySymbolicName);

	return pSuite;
}
