//
// NetTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "NetTestSuite.h"
#include "NetBundleActivatorTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* NetTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("NetTestSuite");

	pSuite->addTest(NetBundleActivatorTest::suite());

	return pSuite;
}
