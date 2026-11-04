//
// AuthRemotingTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthRemotingTestSuite.h"
#include "AuthRemotingTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* AuthRemotingTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthRemotingTestSuite");

	pSuite->addTest(AuthRemotingTest::suite());

	return pSuite;
}
