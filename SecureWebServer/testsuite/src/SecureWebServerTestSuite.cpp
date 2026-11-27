//
// SecureWebServerTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SecureWebServerTestSuite.h"
#include "SecureWebServerBundleActivatorTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* SecureWebServerTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SecureWebServerTestSuite");

	pSuite->addTest(SecureWebServerBundleActivatorTest::suite());

	return pSuite;
}
