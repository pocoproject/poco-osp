//
// AuthCommandTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthCommandTestSuite.h"
#include "AuthCommandTest.h"
#include "PasswdCommandTest.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/Test.h"


CppUnit::Test* AuthCommandTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthCommandTestSuite");

	pSuite->addTest(AuthCommandTest::suite());
	pSuite->addTest(PasswdCommandTest::suite());

	return pSuite;
}
