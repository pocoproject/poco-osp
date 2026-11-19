//
// OSPUnitTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "OSPUnitTestSuite.h"
#include "TestCaseTest.h"
#include "TestFactoryTest.h"


CppUnit::Test* OSPUnitTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("OSPUnitTestSuite");

	pSuite->addTest(TestCaseTest::suite());
	pSuite->addTest(TestFactoryTest::suite());

	return pSuite;
}
