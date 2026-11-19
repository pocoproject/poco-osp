//
// JWTTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "JWTTestSuite.h"
#include "JWTValidatorTest.h"
#include "JWTBundleActivatorTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* JWTTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("JWTTestSuite");

	pSuite->addTest(JWTValidatorTest::suite());
	pSuite->addTest(JWTBundleActivatorTest::suite());

	return pSuite;
}
