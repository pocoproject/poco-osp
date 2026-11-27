//
// SimpleAuthTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SimpleAuthTestSuite.h"
#include "SimpleAuthTest.h"


CppUnit::Test* SimpleAuthTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SimpleAuthTestSuite");

	pSuite->addTest(SimpleAuthTest::suite());

	return pSuite;
}
